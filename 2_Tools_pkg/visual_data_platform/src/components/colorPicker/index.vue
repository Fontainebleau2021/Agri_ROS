<template>
  <a-popover>
    <template slot="content">
      <sketch-picker v-model="color" />
    </template>
    <div
      :style="{
        width:'100%',
        height:'30px',
        background: color.hex || color,
        borderRadius: '4px',
        border:'1px solid #D9D9D9'
      }"
    />
  </a-popover>
</template>

<script>
import { Compact } from 'vue-color'

export default {
  components: {
    'sketch-picker': Compact
  },

  props: {
    defaultColor: {
      type: [String, Object],
      default: '#000000'
    }
  },

  data () {
    return {
      color: ''
    }
  },

  watch: {
    color (val) {
      this.$emit('change', val.hex || val)
    }
  },

  created () {
    this.color = this.defaultColor
  }
}
</script>

<style>

</style>
