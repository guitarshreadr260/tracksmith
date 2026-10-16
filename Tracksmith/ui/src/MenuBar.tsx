import React,{useEffect,useRef,useState} from 'react';
import {menuNames,type StudioCommand} from './commands';
import './menus.css';
export function MenuBar({commands,execute,children}:{commands:StudioCommand[];execute:(id:string)=>boolean;children?:React.ReactNode}){
 const [open,setOpen]=useState<number|null>(null),root=useRef<HTMLElement>(null);
 const triggers=useRef<(HTMLButtonElement|null)[]>([]);
 function close(restore=false){setOpen(null);if(restore&&open!==null)triggers.current[open]?.focus()}
 useEffect(()=>{const outside=(e:PointerEvent)=>{if(!root.current?.contains(e.target as Node))setOpen(null)};document.addEventListener('pointerdown',outside);return()=>document.removeEventListener('pointerdown',outside)},[]);
 useEffect(()=>{if(open!==null)root.current?.querySelector<HTMLButtonElement>('[role="menu"] button:not(:disabled)')?.focus()},[open]);
 function key(e:React.KeyboardEvent){
  if(e.key==='Escape'){e.preventDefault();e.stopPropagation();close(true);return}
  if(e.key==='Tab'){close();return}
  const index=open??Math.max(0,triggers.current.indexOf(document.activeElement as HTMLButtonElement));
  if(e.key==='ArrowRight'||e.key==='ArrowLeft'){e.preventDefault();e.stopPropagation();const next=(index+(e.key==='ArrowRight'?1:9))%10;if(open!==null)setOpen(next);else triggers.current[next]?.focus();return}
  if(['ArrowDown','ArrowUp','Home','End'].includes(e.key)){e.preventDefault();e.stopPropagation();if(open===null){setOpen(index);return}const items=Array.from(root.current!.querySelectorAll<HTMLButtonElement>('[role="menu"] button:not(:disabled)'));if(!items.length)return;const current=items.indexOf(document.activeElement as HTMLButtonElement);items[e.key==='Home'?0:e.key==='End'?items.length-1:(current+(e.key==='ArrowUp'?-1:1)+items.length)%items.length]?.focus()}
 }
 return <nav className="classic-menu" aria-label="Main menu" ref={root} onKeyDown={key} onBlur={e=>{if(!e.currentTarget.contains(e.relatedTarget as Node))setOpen(null)}}><div role="menubar" aria-label="Studio commands" className="daw-menubar">{menuNames.map((name,index)=><div className="daw-menu" key={name} onPointerEnter={()=>{if(open!==null)setOpen(index)}}><button ref={el=>{triggers.current[index]=el}} role="menuitem" aria-haspopup="menu" aria-expanded={open===index} aria-controls={`menu-${name}`} tabIndex={open===null?(index===0?0:-1):-1} onClick={()=>setOpen(open===index?null:index)}>{name}</button>{open===index&&<div id={`menu-${name}`} role="menu" aria-label={name} className="menu-popup">{commands.filter(c=>c.menu===name).map(c=><button key={c.id} role={c.checked===undefined?'menuitem':'menuitemcheckbox'} aria-checked={c.checked} disabled={!c.enabled} onClick={()=>{execute(c.id);close(true)}}><span className="menu-check" aria-hidden="true">{c.checked?'✓':''}</span><span>{c.label}</span><kbd>{c.shortcut}</kbd></button>)}</div>}</div>)}</div><span/>{children}</nav>
}
