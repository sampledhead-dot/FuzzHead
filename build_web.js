const fs=require('fs');const path=require('path');
const dir=path.join(__dirname,'web');
const tpl=fs.readFileSync(path.join(dir,'index.template.html'),'utf8');
const dsp=fs.readFileSync(path.join(dir,'dsp.js'),'utf8');
if(dsp.includes('</script'))throw new Error('dsp.js contiene </script');
fs.mkdirSync(path.join(__dirname,'dist'),{recursive:true});
const out=tpl.replace('/*__DSP__*/',()=>dsp);
const ver=(out.match(/APP_VERSION = '([^']+)'/)||[])[1];
for(const f of fs.readdirSync(path.join(__dirname,'dist')))if(/^fuzzhead-v.*\.html$/.test(f))fs.unlinkSync(path.join(__dirname,'dist',f));
fs.writeFileSync(path.join(__dirname,'dist','fuzzhead-v'+ver+'.html'),out);   // standalone para probar en local/móvil
fs.writeFileSync(path.join(__dirname,'index.html'),out);                       // la que publica GitHub Pages
console.log('ok');
