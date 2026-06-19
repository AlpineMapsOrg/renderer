const JS_MAX_TOUCHES = 3;

class WeBIGeoHacks {

  constructor(webgpuCanvas, logWrapper, logElement, spinnerElement, loadingStatusElement) {
    this.logWrapper = logWrapper;
    this.logElement = logElement;
    this.webgpuCanvas = webgpuCanvas;
    this.spinnerElement = spinnerElement;
    this.loadingStatusElement = loadingStatusElement;
    this._readyCallback = null;

    logWrapper.querySelector('#butclearlog').onclick = () => this.clearLog();
    logWrapper.querySelector('#buthidelog').onclick = () => this.hideLog();
    logWrapper.querySelector('#buttouchemulator').onclick = () => {
      // import touch-emulator.js
      const script = document.createElement('script');
      script.src = 'touch-emulator.js';
      document.head.appendChild(script);
      script.onload = () => { TouchEmulator(); }
      logWrapper.querySelector('#buttouchemulator').style.display = 'none';
    };

    window.addEventListener('keydown', (event) => this.handleKeydownEvent(event));

    // Not implemented with SDL yet. Also maybe there is a better way to handle this. (create SDL events?)
    // webgpuCanvas.addEventListener('mousedown', (event) => this.handleMousedownEvent(event));

    // Prevent right-click context menu
    webgpuCanvas.addEventListener('contextmenu', (event) => { event.preventDefault(); event.stopPropagation(); });
    this.hideLog();
  }

  async handleKeydownEvent(event) {
    // Toggle display of the log by pressing F10
    if (event.key === 'Dead') this.toggleLog();
  }

  async setEminstance(eminstance) {
    this.eminstance = eminstance;
    this.debugBuild = eminstance.hasOwnProperty("getCallStack") || eminstance.hasOwnProperty("getCallstack");
  }

  async checkWebGPU() {
    this.webgpuAvailable = true;
    this.webgpuTimingsAvailable = false;

    if (navigator.gpu === undefined) {
      this.webgpuAvailable = false;
      return;
    }

    const adapter = await navigator.gpu.requestAdapter();
    if (!adapter) {
      this.webgpuAvailable = false;
      return;
    }
    const device = await adapter.requestDevice({ requiredFeatures: ['timestamp-query'] });
    if (!device) {
      this.webgpuAvailable = false;
      return;
    }

    if (!device.features.has('timestamp-query'))
      return;

    const commandEncoder = device.createCommandEncoder();
    if (typeof commandEncoder.writeTimestamp === 'undefined')
      return;
    this.webgpuTimingsAvailable = true;
  }

  uploadFilesWithDialog(filter, tag, multiple) {
    const fileInput = document.createElement('input');
    fileInput.type = 'file';
    fileInput.multiple = !!multiple;
    if (typeof filter === 'string') fileInput.accept = filter;

    fileInput.addEventListener('change', async function (event) {
      try { eminstance.FS.mkdir('/upload'); } catch (e) {}
      for (const file of Array.from(event.target.files)) {
        const data = await new Promise(resolve => {
          const reader = new FileReader();
          reader.onload = e => resolve(new Uint8Array(e.target.result));
          reader.readAsArrayBuffer(file);
        });
        const dstFileName = '/upload/' + file.name;
        eminstance.FS.writeFile(dstFileName, data);
        await eminstance.ccall('global_file_uploaded', null,
          ['string', 'string'], [dstFileName, tag], { async: true });
      }
      fileInput.remove();
    });

    document.body.appendChild(fileInput);
    fileInput.click();
  }

  downloadFile(path, mime) {
    try {
      const data = eminstance.FS.readFile(path);
      const blob = new Blob([data], { type: mime || 'application/octet-stream' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = path.split('/').pop();
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    } catch (e) {
      console.error('downloadFile failed:', e);
    }
  }

  onWasmLoaded(readyCallback) {
    this._readyCallback = readyCallback;
  }

  _stripAnsi(text) {
    return text.replace(/\x1b\[\d+m/g, '');
  }

  _fadeOutSpinner(callback) {
    if (!this.spinnerElement) { callback(); return; }
    this.spinnerElement.addEventListener('transitionend', () => callback(), { once: true });
    this.spinnerElement.classList.add('fade-out');
  }

  log(text) {
    if (this.debug) console.log(text);
    if (this.logElement) {
      let htmlText = this.prepareAnsiLogString(text + '\n');
      this.logElement.innerHTML += htmlText;
      this.logElement.scrollTop = this.logElement.scrollHeight; // focus on bottom
    }
    if (this._readyCallback) {
      const isError = text.includes('\x1b[31m') || text.includes('\x1b[33m');
      if (!isError && this.loadingStatusElement) {
        this.loadingStatusElement.textContent = this._stripAnsi(text).trim();
      }
      if (text.includes('webgpu_app ready')) {
        const cb = this._readyCallback;
        this._readyCallback = null;
        setTimeout(() => this._fadeOutSpinner(cb), 1000);
      }
    }
  }

  showLog() { this.logWrapper.style.display = 'block'; }
  hideLog() { this.logWrapper.style.display = 'none'; }

  toggleLog() {
    if (this.logWrapper.style.display === 'none') this.showLog();
    else this.hideLog();
  }
  clearLog() { this.logElement.innerHTML = ''; }

  prepareAnsiLogString(text) {
    const replacementMap = {
      '&': '&amp;',
      '<': '&lt;',
      '>': '&gt;',
      '\n': '<br>',
      ' ': '&nbsp;',
      '\x1b[30m': '<font color="black">',
      '\x1b[31m': '<font color="#ed4e4c">',
      '\x1b[32m': '<font color="green">',
      '\x1b[33m': '<font color="#d2c057">',
      '\x1b[34m': '<font color="#2774f0">',
      '\x1b[35m': '<font color="magenta">',
      '\x1b[36m': '<font color="#12b5cb">',
      '\x1b[37m': '<font color="white">',
      '\x1b[0m': '</font>' // Reset code to close the font tag
    };

    for (const char in replacementMap) {
      if (replacementMap.hasOwnProperty(char)) {
        const replacement = replacementMap[char];
        text = text.split(char).join(replacement);
      }
    }

    return text;
  }

}
