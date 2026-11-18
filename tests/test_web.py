import asyncio, sys, os
from playwright.async_api import async_playwright
HTML = sys.argv[1]; WAV = sys.argv[2]; OUT = sys.argv[3]
async def main():
    async with async_playwright() as p:
        b = await p.chromium.launch(executable_path='/usr/bin/google-chrome', args=['--autoplay-policy=no-user-gesture-required','--use-fake-ui-for-media-stream','--use-fake-device-for-media-stream'])
        ctx = await b.new_context(accept_downloads=True, viewport={'width':412,'height':900}, device_scale_factor=2)
        pg = await ctx.new_page()
        errs = []
        pg.on('pageerror', lambda e: errs.append(str(e)))
        await pg.goto('file://' + HTML)
        await pg.screenshot(path=OUT+'/01_inicio.png', full_page=True)
        # Subir archivo
        await pg.set_input_files('#file', WAV)
        await pg.wait_for_function("document.getElementById('status').textContent.startsWith('Listo')", timeout=30000)
        print('archivo:', await pg.text_content('#status'))
        # Cada motor
        for e in ['deep','vintage','solo','strings','harmony']:
            await pg.click(f'#engineChips .chip[data-v="{e}"]')
            await pg.wait_for_timeout(400)
            await pg.wait_for_function("document.getElementById('status').textContent.startsWith('Listo')", timeout=30000)
            print(e, '->', await pg.text_content('#status'))
        # Riff de prueba + bajo
        await pg.click('#instSeg button[data-v="bass"]')
        await pg.click('#demoBtn')
        await pg.wait_for_timeout(400)
        await pg.wait_for_function("document.getElementById('status').textContent.startsWith('Listo')", timeout=30000)
        print('riff bajo:', await pg.text_content('#status'))
        # Todos los efectos via preset
        await pg.select_option('#presetSel', 'f:Campanas metálicas')
        await pg.wait_for_timeout(400)
        await pg.wait_for_function("document.getElementById('status').textContent.startsWith('Listo')", timeout=30000)
        print('preset:', await pg.text_content('#status'))
        await pg.click('#m-ring .order button[data-dir="1"]')
        print('orden:', await pg.text_content('#flowline'))
        await pg.wait_for_timeout(400)
        await pg.wait_for_function("document.getElementById('status').textContent.startsWith('Listo')", timeout=30000)
        # Cada interruptor en varios presets (incluido Harmony) y que recalcule el audio
        for preset in ['f:Default','f:Campanas metálicas','f:Quintas y octava']:
            await pg.select_option('#presetSel', preset)
            await pg.wait_for_timeout(400)
            await pg.wait_for_function("document.getElementById('status').textContent.startsWith('Listo')", timeout=30000)
            res = []
            for k in ['synthOn','distOn','modOn','crushOn','ringOn','revOn','int2On']:
                sel = f'.sw[data-p="{k}"]'
                if not await pg.locator(sel).is_visible(): continue
                before = await pg.evaluate(f"P.{k}")
                job = await pg.evaluate("jobId")
                await pg.locator(sel).click()
                await pg.wait_for_timeout(450)
                await pg.wait_for_function("document.getElementById('status').textContent.startsWith('Listo')", timeout=30000)
                ok = (await pg.evaluate(f"P.{k}")) != before and (await pg.evaluate("jobId")) > job
                await pg.locator(sel).click()
                res.append(f"{k}:{'OK' if ok else 'FALLA'}")
                if not ok: raise SystemExit('interruptor ' + k + ' falla en ' + preset)
            print('interruptores', preset, ' '.join(res))
        # Reproducir
        await pg.click('#playBtn'); await pg.wait_for_timeout(600)
        print('play:', await pg.text_content('#playBtn'))
        await pg.click('#playBtn')
        # WAV
        async with pg.expect_download() as d:
            await pg.click('#dlBtn')
        dl = await d.value; path = OUT + '/' + dl.suggested_filename; await dl.save_as(path)
        print('wav:', dl.suggested_filename, os.path.getsize(path))
        # Knob drag
        box = await pg.locator('#synthKnobs svg').bounding_box()
        await pg.mouse.move(box['x']+30, box['y']+30); await pg.mouse.down(); await pg.mouse.move(box['x']+30, box['y']+80, steps=8); await pg.mouse.up()
        print('knob synthMix:', await pg.text_content('#synthKnobs .val'))
        await pg.screenshot(path=OUT+'/02_procesado.png', full_page=True)
        # Grabación con micro falso
        await pg.click('#recBtn'); await pg.wait_for_timeout(1500); await pg.click('#recBtn')
        await pg.wait_for_function("document.getElementById('srcInfo').textContent.startsWith('grabacion')", timeout=15000)
        print('grabación:', await pg.text_content('#srcInfo'))
        # Error forzado -> informe automático
        async with pg.expect_download() as d2:
            await pg.evaluate("setTimeout(()=>{throw new Error('error de prueba')},0)")
        rep = await d2.value; rp = OUT + '/' + rep.suggested_filename; await rep.save_as(rp)
        txt = open(rp, encoding='utf-8').read()
        print('informe:', rep.suggested_filename, len(txt.splitlines()), 'líneas; contiene versión:', 'Versión: v' in txt, '; stack:', 'error de prueba' in txt)
        await ctx.close()
        # escritorio
        ctx2 = await b.new_context(viewport={'width':1280,'height':900})
        pg2 = await ctx2.new_page(); await pg2.goto('file://' + HTML)
        await pg2.set_input_files('#file', WAV)
        await pg2.wait_for_function("document.getElementById('status').textContent.startsWith('Listo')", timeout=30000)
        await pg2.click('#engineChips .chip[data-v="harmony"]'); await pg2.wait_for_timeout(300)
        await pg2.screenshot(path=OUT+'/03_escritorio.png', full_page=True)
        print('errores de página (el forzado es esperado):', errs)
        await b.close()
asyncio.run(main())
