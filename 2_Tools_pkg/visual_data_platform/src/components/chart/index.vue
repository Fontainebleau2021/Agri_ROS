<template>
  <!-- 外包裹器 -->
  <div class="fill-parent">
    <!-- 缩放监听 -->
    <resize-observer @notify="handleResize" />
    <!-- 绘图区域 -->
    <div
      ref="chartZone"
      class="fill-parent"
    />
  </div>
</template>

<script>
const echarts = require('echarts')

export default {
  props: {
    options: {
      type: Object, required: true
    }
  },

  watch: {
    options () {
      this.chart.setOption(this.options)
    }
  },

  mounted () {
    // 初始化绘图
    const chart = echarts.init(this.$refs.chartZone)
    chart.setOption(this.options)
    this.chart = chart
  },

  beforeDestroy () {
    // 销毁组件的时候销毁绘图
    if (this.chart) {
      this.destroy()
    }
  },

  methods: {
    // 画布大小发生变化时候触发
    handleResize () {
      this.chart.resize()
    },
    // 销毁绘画
    destroy () {
      this.chart.dispose()
      this.chart = null
    },
    reInit () {
      this.chart.resize({
        height: 400, width: 600
      })
      setTimeout(() => {
        this.chart.resize({
          height: 'auto', width: 'auto'
        })
      }, 100)
    },
    // 更新配置
    updateOptions (options) {
      this.chart.setOption(options)
    }
  }
}
</script>
