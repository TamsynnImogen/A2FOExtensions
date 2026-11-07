extern crate a2fmt;
use std::{env,fs,path::Path};
fn scan(dir:&Path, stats:&mut [usize;3]) {
 let Ok(entries)=fs::read_dir(dir) else{return};
 for item in entries.flatten() { let p=item.path(); if p.is_dir(){scan(&p,stats);continue;}
 let ext=p.extension().and_then(|x|x.to_str()).unwrap_or("").to_ascii_lowercase();
 if ext!="tga" && ext!="dds" {continue;}
 let data=fs::read(&p).unwrap();
 let decoded=if ext=="dds"{stats[1]+=1;a2fmt::dds::decode(&data).map_err(|e|format!("{e:?}"))}else{stats[0]+=1;a2fmt::tga::decode(&data).map_err(|e|format!("{e:?}"))};
 match decoded {Ok(img)=>{assert_eq!(img.pixels.len(),img.width as usize*img.height as usize*4);},Err(e)=>{stats[2]+=1;println!("FAIL {}: {e}",p.display());}}
 }
}
fn main(){let mut stats=[0;3];for arg in env::args().skip(1){scan(Path::new(&arg),&mut stats);}
 println!("TGA={} DDS={} decode_failures={}",stats[0],stats[1],stats[2]);if stats[2]>0{std::process::exit(1)}}
