export const menuNames = ['File','Edit','View','Track','Clip','MIDI','Audio','Options','Window','Help'] as const;
export type MenuName = typeof menuNames[number];
export type StudioCommand = {id:string; menu:MenuName; label:string; shortcut?:string; enabled:boolean; checked?:boolean; run:()=>void};
/** All command entry points re-check availability against the current render. */
export function createCommandRegistry(commands:StudioCommand[]) {
 const byId=new Map<string,StudioCommand>();
 for(const command of commands){if(byId.has(command.id))throw new Error(`Duplicate command: ${command.id}`);byId.set(command.id,command)}
 return {commands, execute(id:string){const command=byId.get(id);if(!command?.enabled)return false;command.run();return true}};
}
