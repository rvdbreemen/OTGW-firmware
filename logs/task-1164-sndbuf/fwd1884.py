import asyncio
async def pipe(r, w):
    try:
        while True:
            d = await r.read(65536)
            if not d: break
            w.write(d); await w.drain()
    except Exception: pass
    finally:
        try: w.close()
        except Exception: pass
async def handle(cr, cw):
    try:
        br, bw = await asyncio.open_connection("127.0.0.1", 1885)
    except Exception:
        cw.close(); return
    await asyncio.gather(pipe(cr, bw), pipe(br, cw))
async def main():
    srv = await asyncio.start_server(handle, "0.0.0.0", 1884)
    async with srv: await srv.serve_forever()
asyncio.run(main())
