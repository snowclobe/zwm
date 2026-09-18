//! Fetches a random wallpaper from Wallhaven's open API and sets it as the
//! X11 root background via `xwallpaper` (preferred) or `feh`.
//!
//! Network I/O is delegated to the system `curl` binary rather than pulling
//! in a Rust TLS stack: curl is already a build/runtime dependency of this
//! project (used to install rustup), and shelling out to it keeps this
//! crate's own dependency footprint to just JSON parsing.

use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};

/// categories=100 (general only) + purity=100 (sfw only) is *not* enough on
/// its own: Wallhaven's "general" category is a catch-all that also covers
/// character-focused digital art, which turned up distinctly NSFW-adjacent
/// results in testing despite the sfw-only purity flag. Pinning the search
/// to a topic (`q=`) is what actually keeps results wallpaper-appropriate,
/// so we rotate through a short list of safe, scenic topics instead of
/// leaving the query open. See https://wallhaven.cc/help/api for the full
/// parameter reference.
const WALLHAVEN_TOPICS: &[&str] =
    &["nature", "landscape", "mountains", "space", "architecture", "minimal", "ocean", "forest"];

fn random_topic() -> &'static str {
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.subsec_nanos())
        .unwrap_or(0);
    WALLHAVEN_TOPICS[nanos as usize % WALLHAVEN_TOPICS.len()]
}

fn wallhaven_search_url() -> String {
    format!(
        "https://wallhaven.cc/api/v1/search?categories=100&purity=100&sorting=random&per_page=1&q={}",
        random_topic()
    )
}

fn curl_get(url: &str) -> Result<Vec<u8>, String> {
    let output = Command::new("curl")
        .args(["-sL", "--fail", "--max-time", "15", url])
        .output()
        .map_err(|e| format!("failed to run curl: {e}"))?;
    if !output.status.success() {
        return Err(format!(
            "curl exited with {}: {}",
            output.status,
            String::from_utf8_lossy(&output.stderr).trim()
        ));
    }
    Ok(output.stdout)
}

fn fetch_wallhaven_image_url() -> Result<String, String> {
    let body = curl_get(&wallhaven_search_url())?;
    let json: serde_json::Value =
        serde_json::from_slice(&body).map_err(|e| format!("bad JSON from wallhaven: {e}"))?;
    json["data"][0]["path"]
        .as_str()
        .map(|s| s.to_string())
        .ok_or_else(|| "wallhaven response had no data[0].path".to_string())
}

fn cache_dir() -> PathBuf {
    let home = env::var("HOME").unwrap_or_else(|_| "/tmp".to_string());
    PathBuf::from(home).join(".cache").join("zovwm")
}

fn extension_from_url(url: &str) -> &'static str {
    match url.rsplit('.').next().unwrap_or("").to_lowercase().as_str() {
        "png" => "png",
        "jpeg" => "jpeg",
        _ => "jpg",
    }
}

fn try_run(cmd: &str, args: &[&str]) -> bool {
    Command::new(cmd).args(args).status().map(|s| s.success()).unwrap_or(false)
}

fn apply_wallpaper(path: &Path) -> Result<(), String> {
    let path_str = path.to_string_lossy().to_string();
    if try_run("xwallpaper", &["--zoom", &path_str]) {
        return Ok(());
    }
    if try_run("feh", &["--bg-fill", &path_str]) {
        return Ok(());
    }
    Err("neither xwallpaper nor feh is installed — install one to set the wallpaper".to_string())
}

fn main() {
    let dir = cache_dir();
    if let Err(e) = fs::create_dir_all(&dir) {
        eprintln!("zovwm-wallpaper: cannot create cache dir {}: {e}", dir.display());
        std::process::exit(1);
    }

    let mut target: Option<PathBuf> = None;

    match fetch_wallhaven_image_url() {
        Ok(url) => match curl_get(&url) {
            Ok(bytes) if !bytes.is_empty() => {
                let ext = extension_from_url(&url);
                let final_path = dir.join(format!("wallpaper.{ext}"));
                let tmp_path = dir.join(format!("wallpaper.{ext}.tmp"));
                if let Err(e) = fs::write(&tmp_path, &bytes) {
                    eprintln!("zovwm-wallpaper: cannot write {}: {e}", tmp_path.display());
                } else if let Err(e) = fs::rename(&tmp_path, &final_path) {
                    eprintln!("zovwm-wallpaper: cannot rename into place: {e}");
                } else {
                    println!("zovwm-wallpaper: fetched new wallpaper from {url}");
                    target = Some(final_path);
                }
            }
            Ok(_) => eprintln!("zovwm-wallpaper: downloaded image was empty"),
            Err(e) => eprintln!("zovwm-wallpaper: could not download image: {e}"),
        },
        Err(e) => eprintln!("zovwm-wallpaper: could not reach wallhaven: {e}"),
    }

    if target.is_none() {
        // Network (or the API) failed — fall back to whatever's cached
        // from a previous run rather than leaving the screen blank.
        target = ["jpg", "jpeg", "png"]
            .iter()
            .map(|ext| dir.join(format!("wallpaper.{ext}")))
            .find(|p| p.exists());
        if target.is_some() {
            eprintln!("zovwm-wallpaper: falling back to cached wallpaper");
        }
    }

    let Some(path) = target else {
        eprintln!("zovwm-wallpaper: no wallpaper available (no network and no cache)");
        std::process::exit(1);
    };

    if let Err(e) = apply_wallpaper(&path) {
        eprintln!("zovwm-wallpaper: {e}");
        std::process::exit(1);
    }
}
