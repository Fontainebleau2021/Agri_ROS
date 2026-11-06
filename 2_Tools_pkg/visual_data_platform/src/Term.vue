<template>
  <div
    id="app"
    style="width: 100vw; height: 100vh; overflow: hidden;"
  >
    <div
      id="terminal"
      style="width: 100%; height: 100%; overflow: hidden;"
      @contextmenu="copySelected"
    />
    <resize-observer @notify="handleResize" />
  </div>
</template>

<script>
import xtermTheme from 'xterm-theme'
const Terminal = window.require('xterm').Terminal
const { FitAddon } = window.require('xterm-addon-fit')

export default {
  data () {
    return {
      validDate: ''
    }
  },

  mounted () {
    const term = new Terminal({
      fontSize: 16,
      theme: xtermTheme.Violet_Light,
      fontFamily: 'consolas, inconsolata, menlo',
      letterSpacing: 0
    })

    this.fitAddon = new FitAddon()
    term.loadAddon(this.fitAddon)
    term.open(document.getElementById('terminal'))
    this.fitAddon.fit()

    const pty = window.require('node-pty')

    const ptyProcess = pty.spawn('powershell.exe', [], {
      cols: term.cols,
      rows: term.rows,
      cwd: process.env.HOME
    })

    term.onResize(size => {
      ptyProcess.resize(
        Math.max(size ? size.cols : term.cols, 1),
        Math.max(size ? size.rows : term.rows, 1)
      )
    })

    term.onData((data) => {
      ptyProcess.write(data)
    })

    ptyProcess.on('data', data => {
      term.write(data)
    })

    this.term = term
    this.ptyProcess = ptyProcess
  },

  beforeDestroy () {
    this.term && this.term.destroy()
  },

  methods: {
    handleResize () {
      this.fitAddon && this.fitAddon.fit()
    },
    copySelected () {
      if (this.term.hasSelection()) {
        this.$clipboard(this.term.getSelection())
        this.term.clearSelection()
      } else {
        navigator.clipboard.readText()
          .then(text => {
            this.ptyProcess.write(text)
          })
      }
    }
  }
}
</script>
