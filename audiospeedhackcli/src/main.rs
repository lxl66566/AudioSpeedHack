use std::time::Duration;

use dll_syringe::{process::OwnedProcess, Syringe};

fn main() {
    // find target process by name
    let target_process = OwnedProcess::find_first_by_name("python").unwrap();

    // create a new syringe for the target process
    let syringe = Syringe::for_process(target_process);

    // inject the payload into the target process
    let injected_payload = syringe.inject("target/release/speedhacklib.dll").unwrap();

    // do something else
    std::thread::sleep(Duration::from_secs(1000));

    // eject the payload from the target (optional)
    syringe.eject(injected_payload).unwrap();
}
