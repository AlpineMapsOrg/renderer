const JS_MAX_TOUCHES = 3;

class WeBIGeoHacks {

  constructor(webgpuCanvas, logWrapper, logElement) {
    this.logWrapper = logWrapper;
    this.logElement = logElement;
    this.webgpuCanvas = webgpuCanvas;

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

  uploadFileWithDialog(filter, tag) {
    // Create a file input element if not already created
    var fileInput = document.getElementById('fileInputDialog');
    if (!fileInput) {
      var fileInput = document.createElement('input');
      fileInput.id = "fileInputDialog";
      fileInput.type = 'file';

      fileInput.addEventListener('change', function (event) {
        var file = event.target.files[0];  // Get the selected file
        var reader = new FileReader();

        reader.onload = async function (e) {
          var data = new Uint8Array(e.target.result);
          let dstFileName = "/upload/" + file.name;

          // Create the upload directory if not existing yet
          try {
            eminstance.FS.mkdir('/upload');
          } catch (e) { }
          // Write file to Emscripten's virtual file system
          eminstance.FS.writeFile(dstFileName, data);

          // Call the global_file_uploaded function in the Emscripten module (WebInterop)
          await eminstance.ccall("global_file_uploaded", null, ["string", "string"], [dstFileName, tag], { async: true });

          fileInput.remove();
        };

        reader.readAsArrayBuffer(file);  // Read file as ArrayBuffer
      });
    }
    if (typeof filter === 'string') {
      fileInput.accept = filter;
    }
    fileInput.click();
  }

  log(text) {
    if (this.debug) console.log(text);
    if (this.logElement) {
      let htmlText = this.prepareAnsiLogString(text + '\n');
      this.logElement.innerHTML += htmlText;
      this.logElement.scrollTop = this.logElement.scrollHeight; // focus on bottom
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
