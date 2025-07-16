use std::fs::File;
use std::io::BufReader;
use std::time::{SystemTime, Duration};
use chrono::{DateTime, Local, TimeZone, Duration as ChronoDuration};
use serde::{Deserialize, Serialize};
use serde_json;

#[derive(Serialize, Deserialize)]
struct DateEntry {
    name: String,
    date: String,
}

#[derive(Serialize, Deserialize)]
struct DatesWrapper {
    holidays: Vec<DateEntry>,
    events: Vec<DateEntry>,
    birthdays: Vec<DateEntry>,
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Get the current system time
    let now = SystemTime::now();
    let datetime: DateTime<Local> = now.into();
    
    // Format the date and time similar to Unix date command
    let formatted = datetime.format("%Y-%b-%d %a %H:%M:%S %z").to_string();
    println!("Date: {}", formatted);
    
    // Format today's date as MM-DD for comparison
    let today = datetime.format("%m-%d").to_string();
    
    // Read and parse the dates JSON file
    let file = File::open("dates.json")?;
    let reader = BufReader::new(file);
    let wrapper: DatesWrapper = serde_json::from_reader(reader)?;
    
    // Check for today's holidays, events, and birthdays
    let mut today_found = false;
    for entry in wrapper.holidays.iter().filter(|e| e.date == today) {
        println!("Holiday: {}", entry.name);
        today_found = true;
    }
    for entry in wrapper.events.iter().filter(|e| e.date == today) {
        println!("Event: {}", entry.name);
        today_found = true;
    }
    for entry in wrapper.birthdays.iter().filter(|e| e.date == today) {
        println!("Birthday: {}", entry.name);
        today_found = true;
    }
    if !today_found {
        println!("No holidays, events, or birthdays today.");
    }
    println!("---------------------------------------------------------");
    
    // Check for upcoming holidays, events, and birthdays (next 7 days)
    println!("Upcoming in the next 7 days:");
    let mut upcoming_found = false;
    for days in 1..=7 {
        let future_date = datetime + ChronoDuration::days(days);
        let future_mm_dd = future_date.format("%m-%d").to_string();
        
        for entry in wrapper.holidays.iter().filter(|e| e.date == future_mm_dd) {
            println!("{} ({}): Holiday - {}", future_date.format("%b %d"), future_mm_dd, entry.name);
            upcoming_found = true;
        }
        for entry in wrapper.events.iter().filter(|e| e.date == future_mm_dd) {
            println!("{} ({}): Event - {}", future_date.format("%b %d"), future_mm_dd, entry.name);
            upcoming_found = true;
        }
        for entry in wrapper.birthdays.iter().filter(|e| e.date == future_mm_dd) {
            println!("{} ({}): Birthday - {}", future_date.format("%b %d"), future_mm_dd, entry.name);
            upcoming_found = true;
        }
    }
    if !upcoming_found {
        println!("No holidays, events, or birthdays in the next 7 days.");
    }
    
    Ok(())
}
