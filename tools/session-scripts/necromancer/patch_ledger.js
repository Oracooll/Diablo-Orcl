const fs = require('fs');
const p = process.argv[2];
let s = fs.readFileSync(p, 'utf8');
function rep(a, b) { if (!s.includes(a)) throw new Error('miss: ' + a.slice(0, 60)); s = s.replace(a, () => b); }

// ---- styles ----
rep('dialog{border:1px solid', `/* skill ledger */
.tabs{display:flex;flex-wrap:wrap;gap:6px;margin:0 0 14px;border-bottom:1px solid var(--line)}
.tab{background:none;border:none;border-bottom:2px solid transparent;padding:8px 14px;font-size:14.5px;color:var(--muted);margin-bottom:-1px}
.tab:hover{color:var(--ink)}
.tab.on{color:var(--ink);border-bottom-color:var(--accent);font-weight:600}
.tab small{font-family:var(--mono);font-size:11px;color:var(--bone);margin-left:6px}
.tree{display:grid;grid-template-columns:64px repeat(3,minmax(0,1fr));gap:10px;min-width:640px}
.tier{font-family:var(--mono);font-size:11px;letter-spacing:.06em;text-transform:uppercase;color:var(--bone);padding-top:14px}
.cell{display:flex;flex-direction:column;gap:6px;text-align:left;background:var(--surface);border:1px solid var(--line);border-radius:8px;padding:12px 14px;min-height:112px;color:var(--ink)}
button.cell:hover{border-color:var(--accent);background:var(--raised)}
.cell.hole{border-style:dashed;background:transparent;color:var(--muted);font-size:13px;justify-content:center;align-items:center}
.cell.keep{box-shadow:inset 3px 0 0 var(--ok)} .cell.change{box-shadow:inset 3px 0 0 var(--warn)} .cell.replace{box-shadow:inset 3px 0 0 var(--accent)}
.cell b{font-family:var(--display);font-weight:600;font-size:16.5px;line-height:1.2}
.cell p{margin:0;font-size:13px;line-height:1.45;color:var(--muted);display:-webkit-box;-webkit-line-clamp:3;-webkit-box-orient:vertical;overflow:hidden}
.chips{display:flex;flex-wrap:wrap;gap:5px;margin-top:auto}
.chip{font-family:var(--mono);font-size:10.5px;letter-spacing:.05em;text-transform:uppercase;padding:2px 7px;border-radius:3px;background:var(--chip);color:var(--muted)}
.chip.essence{color:var(--ok)} .chip.mana{color:var(--accent)} .chip.v{background:var(--ink);color:var(--bg)}
.chip.note{color:var(--warn)}
.row3{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px}
@media (max-width:560px){.row3{grid-template-columns:1fr}}
.hint{font-size:12.5px;color:var(--muted);margin:2px 0 0}

dialog{border:1px solid`);

// ---- markup ----
rep('  <h2>Ideas &amp; tasks', `  <h2>Skill ledger <small id="ledgerCount"></small></h2>
  <p class="sub">His four pages as they stand in the game, laid out as the tree lays them out. Open a skill to rename it, rewrite what it does, move it to another cell, change what it costs, or mark it to be replaced. Nothing here is built yet, so everything is cheap to change - Claude reads this before building each page.</p>
  <div class="tabs" id="tabs" role="tablist"></div>
  <div class="scroll"><div class="tree" id="tree"></div></div>

  <h2>Ideas &amp; tasks`);

rep('<script>', `<dialog id="sDlg">
  <form method="dialog" id="sForm">
    <h3 id="sHead">Skill</h3>
    <label class="field">Name<input id="sName" maxlength="60" autocomplete="off" required></label>
    <label class="field">What it does<textarea id="sDesc" maxlength="1200" style="min-height:110px"></textarea></label>
    <div class="row3">
      <label class="field">Kind<select id="sKind"><option>Active</option><option>Passive</option><option>Aura</option></select></label>
      <label class="field">Paid in<select id="sPays"><option value="mana">Mana</option><option value="essence">Essence</option><option value="none">Nothing</option></select></label>
      <label class="field">Essence cost<input id="sCost" type="number" min="0" max="100" step="1"></label>
    </div>
    <p class="hint">Mana costs are set with the numbers when a skill is built; an Essence cost is out of his pool of 100.</p>
    <div class="row3">
      <label class="field">Page<select id="sPage"></select></label>
      <label class="field">Row (level)<select id="sTier"></select></label>
      <label class="field">Column<select id="sCol"><option value="0">Left</option><option value="1">Middle</option><option value="2">Right</option></select></label>
    </div>
    <p class="hint">Moving onto a cell that is taken swaps the two skills.</p>
    <label class="field">Your verdict<select id="sVerdict"><option value="">Not looked at yet</option><option value="keep">Keep as it is</option><option value="change">Change - see my notes</option><option value="replace">Replace with something else</option></select></label>
    <label class="field">Notes for Claude<textarea id="sNotes" maxlength="3000" placeholder="Numbers you want, how it should feel, what to replace it with..."></textarea></label>
    <div class="saved err" id="sErr" role="alert"></div>
    <div class="actions"><div class="right"><button type="button" class="btn ghost" id="sCancel">Cancel</button><button type="submit" class="btn" id="sSave">Save</button></div></div>
  </form>
</dialog>

<script>`);

// ---- script ----
rep('  /* ---------- storage ---------- */', `  /* ---------- skill ledger ---------- */
  var skills=[], page=0, sEditing=null;
  var PAGES=["Summoning","Poison & Bone","Curses","Passive Skills"], LEVELS=[1,6,12,18,24,30];
  function tierLabel(pg,t){return pg===3?("Row "+(t+1)):("Level "+LEVELS[t]);}
  function at(pg,t,c){for(var i=0;i<skills.length;i++){var k=skills[i];if(k.page===pg&&k.tier===t&&k.column===c){return k;}}return null;}
  function renderTabs(){
    var box=$("tabs");box.textContent="";
    PAGES.forEach(function(name,i){
      var looked=skills.filter(function(k){return k.page===i&&k.verdict;}).length,total=skills.filter(function(k){return k.page===i;}).length;
      var b=el("button","tab"+(i===page?" on":""),name);b.type="button";b.setAttribute("role","tab");b.setAttribute("aria-selected",i===page?"true":"false");
      b.appendChild(el("small",null,looked+"/"+total));
      b.addEventListener("click",function(){page=i;renderLedger();});box.appendChild(b);});
  }
  function renderLedger(){
    renderTabs();
    var looked=skills.filter(function(k){return k.verdict;}).length;
    $("ledgerCount").textContent=skills.length?(looked+" of "+skills.length+" looked at"):"";
    var tree=$("tree");tree.textContent="";
    if(!skills.length){tree.appendChild(el("div","empty","No skills loaded yet."));return;}
    for(var t=0;t<6;t++){
      tree.appendChild(el("div","tier",tierLabel(page,t)));
      for(var c=0;c<3;c++){(function(k){
        if(!k){tree.appendChild(el("div","cell hole","empty cell"));return;}
        var b=el("button","cell"+(k.verdict?" "+k.verdict:""));b.type="button";
        b.appendChild(el("b",null,k.name));b.appendChild(el("p",null,k.description));
        var ch=el("div","chips");ch.appendChild(el("span","chip",k.kind));
        if(k.pays==="essence"){ch.appendChild(el("span","chip essence","Essence "+k.cost));}else if(k.pays==="mana"){ch.appendChild(el("span","chip mana","Mana"));}
        if(k.verdict){ch.appendChild(el("span","chip v",k.verdict));}
        if(k.notes){ch.appendChild(el("span","chip note","note"));}
        b.appendChild(ch);b.addEventListener("click",function(){openSkill(k);});tree.appendChild(b);
      })(at(page,t,c));}
    }
  }
  function fillSelect(sel,labels){sel.textContent="";labels.forEach(function(l,i){var o=document.createElement("option");o.value=String(i);o.textContent=l;sel.appendChild(o);});}
  function paintTierOptions(){var pg=Number($("sPage").value),keep=$("sTier").value;fillSelect($("sTier"),[0,1,2,3,4,5].map(function(t){return tierLabel(pg,t);}));if(keep){$("sTier").value=keep;}}
  fillSelect($("sPage"),PAGES);$("sPage").addEventListener("change",paintTierOptions);
  function paintCost(){$("sCost").disabled=$("sPays").value!=="essence";}
  $("sPays").addEventListener("change",paintCost);
  function openSkill(k){
    sEditing=k.id;$("sHead").textContent=k.name;$("sName").value=k.name;$("sDesc").value=k.description;$("sKind").value=k.kind;$("sPays").value=k.pays;$("sCost").value=String(k.cost||0);
    $("sPage").value=String(k.page);paintTierOptions();$("sTier").value=String(k.tier);$("sCol").value=String(k.column);$("sVerdict").value=k.verdict;$("sNotes").value=k.notes;
    $("sErr").textContent="";paintCost();$("sDlg").showModal();$("sName").focus();
  }
  $("sCancel").addEventListener("click",function(){$("sDlg").close();});
  $("sForm").addEventListener("submit",function(ev){
    ev.preventDefault();if(!db||!sEditing){return;}
    var me=null;skills.forEach(function(k){if(k.id===sEditing){me=k;}});if(!me){return;}
    var name=$("sName").value.trim();if(!name){$("sErr").textContent="Give it a name.";return;}
    var pg=Number($("sPage").value),t=Number($("sTier").value),c=Number($("sCol").value),now=new Date().toISOString();
    var pays=$("sPays").value,cost=pays==="essence"?Math.max(0,Math.min(100,Math.round(Number($("sCost").value)||0))):0;
    var patch={name:name,description:$("sDesc").value.trim(),kind:$("sKind").value,pays:pays,cost:cost,page:pg,tier:t,column:c,verdict:$("sVerdict").value,notes:$("sNotes").value.trim(),updated:now};
    var other=at(pg,t,c);$("sSave").disabled=true;
    var done=function(){$("sSave").disabled=false;page=pg;$("sDlg").close();renderLedger();},bad=function(e){$("sSave").disabled=false;$("sErr").textContent=explain(e);};
    /* The occupant moves to where this skill was FIRST, so a failure between the two writes leaves a doubled cell to fix, never a lost skill. */
    if(other&&other.id!==me.id){db.doc("skills/"+other.id).update({page:me.page,tier:me.tier,column:me.column,updated:now}).then(function(){return db.doc("skills/"+me.id).update(patch);}).then(done,bad);}
    else{db.doc("skills/"+me.id).update(patch).then(done,bad);}
  });

  /* ---------- storage ---------- */`);

rep('    db.collection("notes").orderBy("created","desc")', `    db.collection("skills").orderBy("order").onSnapshot(function(s){
      skills=s.docs.map(function(d){var x=d.data()||{};return {id:d.id,name:String(x.name||d.id),description:String(x.description||""),kind:String(x.kind||"Active"),
        pays:String(x.pays||"none"),cost:Number(x.cost)||0,page:Number(x.page)||0,tier:Number(x.tier)||0,column:Number(x.column)||0,verdict:String(x.verdict||""),notes:String(x.notes||"")};});
      if(!$("sDlg").open){renderLedger();}},fail);
    db.collection("notes").orderBy("created","desc")`);
fs.writeFileSync(p, s);
console.log('ok');
