export const menuNames = ['File','Edit','View','Track','Clip','MIDI','Audio','Options','Window','Help'] as const;
export type MenuName = typeof menuNames[number];
export type StudioCommand = {id:string; menu:MenuName; label:string; shortcut?:string; aliases?:string[]; repeatable?:boolean; enabled:boolean; checked?:boolean; run:()=>void};
export type ShortcutKey = Pick<KeyboardEvent,'code'|'ctrlKey'|'altKey'|'metaKey'|'shiftKey'|'repeat'>;
function matches(shortcut:string,event:ShortcutKey){
 const parts=shortcut.split('+'),key=parts.pop()!;
 const code=key==='='?'Equal':key.length===1?`Key${key.toUpperCase()}`:key;
 return event.code===code&&event.ctrlKey===parts.includes('Ctrl')&&event.altKey===parts.includes('Alt')&&event.metaKey===parts.includes('Meta')&&event.shiftKey===parts.includes('Shift');
}
/** All command entry points re-check availability against the current render. */
export function createCommandRegistry(commands:StudioCommand[]) {
 const byId=new Map<string,StudioCommand>();
 for(const command of commands){if(byId.has(command.id))throw new Error(`Duplicate command: ${command.id}`);byId.set(command.id,command)}
 const execute=(id:string)=>{const command=byId.get(id);if(!command?.enabled)return false;command.run();return true};
 return {commands,execute, shortcut(event:ShortcutKey){const command=commands.find(c=>[c.shortcut,...c.aliases||[]].some(s=>s&&matches(s,event)));if(!command)return false;if(!event.repeat||command.repeatable)execute(command.id);return true}};
}
