/* Own a scenario's temporary runtime. Successful checks clean up; failed
   checks keep the catalog, settings, logs and captures for later diagnosis. */

const { rmSync, writeFileSync } = require('node:fs');
const { join } = require('node:path');

/* Delete only after the whole scenario returns, including its process cleanup.
   A failure reports the retained root and preserves the original stack/cause. */
async function withRuntime(runtime, run) {
  try {
    const result = await run();
    rmSync(runtime, { recursive: true, force: true });
    return result;
  } catch (error) {
    writeFileSync(join(runtime, 'failure.log'), error.stack || String(error));
    throw new Error(`${error.message}\nUI failure artifacts retained at ${runtime}`, { cause: error });
  }
}

module.exports = { withRuntime };
