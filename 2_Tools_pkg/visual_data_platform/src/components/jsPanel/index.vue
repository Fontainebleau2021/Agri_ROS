<template>
  <div>
    <template v-if="keepDom">
      <slot
        v-show="false"
      />
      <slot
        v-show="false"
        name="footerToolbar"
      />
    </template>
    <template v-else-if="visible">
      <slot
        v-show="false"
      />
      <slot
        v-show="false"
        name="footerToolbar"
      />
    </template>
  </div>
</template>

<script>
const { jsPanel } = require('jspanel4')

export default {
  name: 'JsPanel',
  props: {
    visible: {
      type: Boolean,
      default: false
    },
    keepDom: {
      type: Boolean,
      default: false
    },
    modalId: {
      type: String,
      default: 'modal'
    },
    options: {
      type: Object,
      default: () => ({})
    }
  },

  watch: {
    visible (isVisible) {
      if (isVisible) {
        this.createPanel()
      } else {
        this.panel && this.panel.close()
        this.panel = null
      }
    }
  },

  mounted () {
    if (this.visible) {
      this.createPanel()
    }

    if (this.keepDom) {
      this.$emit('afterMounted')
    }
    document.addEventListener('jspanelresizestop', this.resizeHandler, false)
  },

  beforeDestroy () {
    document.removeEventListener('jspanelresizestop', this.resizeHandler, false)
  },

  methods: {
    async createPanel () {
      await this.$nextTick()
      let options = Object.assign(
        {
          onmaximized: this.resize,
          onnormalized: this.resize,
          onparentresize: this.resize,
          id: this.modalId,
          onclosed: this.close,
          content: this.$slots.default[0].elm
        },
        this.options
      )
      if (this.$slots.footerToolbar) {
        options = Object.assign(
          { footerToolbar: this.$slots.footerToolbar[0].elm },
          options
        )
      }
      this.panel = jsPanel.create(options)
      this.$emit('resize')
      if (!this.keepDom) {
        this.$emit('afterMounted')
      }
    },

    close () {
      this.panel = null
      this.$emit('close')
      this.$emit('update:visible', false)
    },

    resize (e) {
      this.$emit('resize')
    },

    resizeHandler ({ panel }) {
      if (panel.id === this.modalId) {
        this.$emit('resize')
      }
    }

  }
}
</script>
