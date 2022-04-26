/* VideoOut.js
 * Manages the <canvas> and incoming framebuffers.
 */
 
export class VideoOut {
  constructor() {
    this.element = null;
    this.context = null;
    this.imageData = null;
  }
  
  /* Create my <canvas> element and attach to (parent).
   */
  setup(parent) {
    this.element = document.createElement("CANVAS");
    this.element.width = 96;
    this.element.height = 64;
    const scale = 6;
    this.element.style.width = `${96 * scale}px`;
    this.element.style.height = `${64 * scale}px`;
    this.element.style.imageRendering = 'crisp-edges';
    parent.appendChild(this.element);
    this.context = this.element.getContext("2d");
    this.imageData = this.context.createImageData(96, 64);
  }
  
  /* Put the new image on screen.
   * (src) must be a Uint8Array of length 96 * 64 * 2, straight off the Wasm app.
   * This part is enormously expensive (and it's a ridiculously small framebuffer too).
   * Between the two things (copying out framebuffer, then putImageData), it burns like 50% CPU on my box.
   * If we're going to do WebAssembly games going forward, don't ever do bulk framebuffer transfer!
   */
  render(src) {
    let srcp = 0, dstp = 0, i = 96 * 64;
    for (; i-->0; dstp+=4, srcp += 2) {
      /* If you're doing color...
      let r = (src[srcp + 1] & 0x1f) << 3; r |= r >> 5;
      let g = ((src[srcp + 1] & 0xe0) >> 3) | ((src[srcp] & 0x07) << 5); g |= g >> 6;
      let b = (src[srcp] & 0xf8); b |= b >> 5;
      /**/
      /* This game happens to be grayscale, so we only need to read one channel.
       */
      let luma = (src[srcp] & 0xf8); luma |= luma >> 5;
      this.imageData.data[dstp + 0] = luma;
      this.imageData.data[dstp + 1] = luma;
      this.imageData.data[dstp + 2] = luma;
      this.imageData.data[dstp + 3] = 0xff;
    }
    this.context.putImageData(this.imageData, 0, 0);
  }
  
}
