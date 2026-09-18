const fs=require('fs'),path=require('path');
const src=process.argv[2],out=process.argv[3];
const md=fs.readFileSync(src,'utf8').split(/\r?\n/);
const now=new Date().toISOString();
const writes=[];const seen=new Set();
for(const line of md){
  if(!line.startsWith('| ')||line.startsWith('| Name |')||line.startsWith('|---'))continue;
  const c=line.replace(/^\|/,'').replace(/\|\s*$/,'').split('|').map(s=>s.trim());
  if(c.length<6)continue;
  const [name,group,size,save,blocked]=c;const summary=c.slice(5).join('|').trim();
  let status='Idea';
  if(/^(PARKED|DEFERRED)/.test(summary))status='Parked';
  if(name==="The Barbarian's Rage")status='Shipped';
  let id=name.toLowerCase().replace(/[^a-z0-9]+/g,'-').replace(/^-|-$/g,'').slice(0,60);
  while(seen.has(id))id+='-2';seen.add(id);
  const doc={name,group,size,save,blocked,summary,status,created:now,updated:now};
  if(status==='Shipped')doc.summary='SHIPPED at v1.12.002 (2026-09-13): the Barbarian has no mana; Rage is built by generators and spent by spenders, carries across levels since v1.12.020. Original note: '+summary;
  const f=path.join(out,String(writes.length)+'.json');fs.writeFileSync(f,JSON.stringify(doc,null,1));
  writes.push({op:'set',collection:'ideas',doc_id:id,file_path:f});
}
fs.writeFileSync(path.join(out,'..','writes.json'),JSON.stringify(writes));
console.log(writes.length,'ideas');
