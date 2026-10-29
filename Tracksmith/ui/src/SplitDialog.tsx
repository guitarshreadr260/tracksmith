import React from 'react';
import type {Command} from './bridge';
import './generation.css';

export function SplitDialog({state,command,onClose}:{state?:{busy:boolean;ready:boolean;message:string};command:(c:Command)=>void;onClose:()=>void}){
 return <div className="generation-dock"><section className="generation-dialog" role="dialog" aria-label="Tracksmith SPLIT">
  <header><div><small>TRACKSMITH SPLIT</small><h2>Separate a song</h2></div><button onClick={onClose} aria-label="Close SPLIT">x</button></header>
  <p>Choose a file to separate on your NVIDIA GPU. Your original file stays unchanged.</p>
  <p>Six stems: vocals, drums, bass, guitar, piano and other. Lead and rhythm guitar are combined.</p>
  <div className="generation-actions"><button disabled={!!state?.busy} onClick={()=>command({type:'splitChoose'})}>Choose audio and split</button><button disabled={!!state?.busy} onClick={()=>command({type:'splitLoad'})}>Load saved stems</button><button disabled={!state?.ready||!!state?.busy} onClick={()=>command({type:'splitInsert'})}>Add stems to session</button><button onClick={()=>command({type:'splitReveal'})}>Open output folder</button></div>
  <p className="generation-status" role="status">{state?.message||'Choose an audio file to begin.'}</p>
  <footer>Stems are added on separate tracks at the beginning of the session. Save the session after importing.</footer>
 </section></div>;
}
