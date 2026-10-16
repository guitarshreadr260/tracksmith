/** Read file-backed plugin/Explorer MIDI drops without trusting path strings. */
export function midiDropFiles(transfer:DataTransfer):File[]{
 const files=Array.from(transfer.files??[]);if(files.length)return files;
 return Array.from(transfer.items??[]).filter(item=>item.kind==='file').map(item=>item.getAsFile()).filter((file):file is File=>!!file);
}
export async function midiDropData(files:File[]){
 if(files.length!==1||!/[.](mid|midi)$/i.test(files[0].name)||files[0].size>1048576)throw new Error(files.length?'Drop one .mid / .midi groove under 1 MiB.':'This plugin did not provide a readable MIDI file. Export the groove as .mid, then drop that file here.');
 try{const bytes=new Uint8Array(await files[0].arrayBuffer());let binary='';for(const byte of bytes)binary+=String.fromCharCode(byte);return btoa(binary);}catch{throw new Error('Could not read this groove. Export it as MIDI and try again.');}
}
