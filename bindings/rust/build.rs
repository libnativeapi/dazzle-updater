use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    // bindings/rust -> core/
    let core_dir = manifest_dir.join("../../core").canonicalize().unwrap();

    println!("cargo:rerun-if-changed={}", core_dir.join("src").display());
    println!(
        "cargo:rerun-if-changed={}",
        core_dir.join("include").display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        core_dir.join("CMakeLists.txt").display()
    );

    let mut config = cmake::Config::new(&core_dir);
    config.define("BUILD_TESTING", "OFF");
    let profile = config.get_profile().to_owned();
    let dst = config.build_target("dazzle_updater").build();

    let lib_dir = dst.join("build").join("src");
    println!("cargo:rustc-link-search=native={}", lib_dir.display());
    // Multi-config generators (Visual Studio, Xcode) add a per-profile directory.
    println!(
        "cargo:rustc-link-search=native={}",
        lib_dir.join(profile).display()
    );
    println!("cargo:rustc-link-lib=static=dazzle_updater");

    // Mirrors the platform libraries in core/src/CMakeLists.txt.
    match env::var("CARGO_CFG_TARGET_OS").unwrap().as_str() {
        "macos" | "ios" => {
            println!("cargo:rustc-link-lib=framework=Foundation");
            println!("cargo:rustc-link-lib=c++");
        }
        "linux" => {
            println!("cargo:rustc-link-lib=stdc++");
            println!("cargo:rustc-link-lib=pthread");
        }
        "android" => {
            println!("cargo:rustc-link-lib=c++_shared");
            println!("cargo:rustc-link-lib=log");
            println!("cargo:rustc-link-lib=android");
        }
        "windows" => {
            println!("cargo:rustc-link-lib=kernel32");
        }
        other => println!("cargo:warning=Unsupported target OS: {other}"),
    }
}
