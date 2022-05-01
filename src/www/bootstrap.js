import { WasmAdapter } from "./js/WasmAdapter.js";
import { VideoOut } from "./js/VideoOut.js";
import { InputManager } from "./js/InputManager.js";
//TODO AudioOut

const wasmAdapter = new WasmAdapter();
const videoOut = new VideoOut();
const inputManager = new InputManager();

function render() {
  const fb = wasmAdapter.getFramebufferIfDirty();
  if (fb) {
    videoOut.render(fb);
  }
  window.requestAnimationFrame(render);
}

window.addEventListener("load", () => {
  wasmAdapter.load("ivand.wasm").then(() => {
    videoOut.setup(document.getElementById("game-container"));
    wasmAdapter.init();
    wasmAdapter.update(0);
    render();
    window.setInterval(() => wasmAdapter.update(inputManager.update()), 1000 / 60);
  }).catch((error) => {
    console.log(`Failed to fetch or load WebAssembly module.`, error);
  });
});
