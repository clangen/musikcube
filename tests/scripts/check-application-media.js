const fs = require('fs');
const path = require('path');
const http = require('http');
const { spawn } = require('child_process');

(async () => {
    const [root, consumer, fixture, decoder, encoder, remote] = process.argv.slice(2);
    fs.mkdirSync(root, { recursive: true });
    const run = fs.mkdtempSync(path.join(root, 'media-'));
    const plugins = path.join(run, 'plugins');
    fs.mkdirSync(plugins);
    for (const source of [fixture, decoder, encoder, remote].filter(Boolean)) {
        fs.copyFileSync(source, path.join(plugins, path.basename(source)));
    }
    const server = http.createServer((request, response) => {
        response.writeHead(200, { 'Content-Type': 'application/octet-stream', 'Content-Length': 5 });
        response.end(Buffer.from([64, 128, 192, 255, 32]));
    });
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    const args = [plugins, path.join(plugins, path.basename(fixture)),
        path.join(run, 'input.bytes'), path.join(run, 'output.bytes')];
    if (remote) {
        args.push(`http://127.0.0.1:${server.address().port}/media.bytes`);
    }
    const child = spawn(consumer, args, { stdio: 'inherit', env: { ...process.env,
        HOME: run, XDG_CONFIG_HOME: run, APPDATA: run } });
    const timer = setTimeout(() => child.kill('SIGKILL'), 25000);
    const status = await new Promise(resolve => child.once('exit', (code, signal) => resolve(signal ? 1 : code)));
    clearTimeout(timer);
    await new Promise(resolve => server.close(resolve));
    if (status) {
        console.error(`application media artifacts: ${run}`);
        process.exitCode = status;
    } else {
        fs.rmSync(run, { recursive: true, force: true });
    }
})().catch(error => { console.error(error); process.exitCode = 1; });
