<template>
  <!-- 外包裹器 -->
  <div class="fill-parent">
    <!-- 缩放监听 -->
    <resize-observer @notify="handleResize" />
    <div
      ref="chart"
    />
  </div>
</template>

<script>
import {
  Line,
  Bar,
  Column,
  DualAxes,
  Gauge,
  Heatmap,
  Histogram,
  Liquid,
  Pie,
  Area,
  Box,
  Scatter,
  Radar,
  Lab,
  Stock,
  Mix
} from '@antv/g2plot'

const getPlot = {
  Line,
  Bar,
  Column,
  DualAxes,
  Gauge,
  Heatmap,
  Histogram,
  Liquid,
  Pie,
  Area,
  Box,
  Scatter,
  Radar,
  Mix,
  Stock,
  MultiView: Lab.MultiView
}

export default {
  props: {
    data: {
      type: Object, required: true
    }
  },

  watch: {
    data () {
      const options = this.computeOptions(this.data)
      this.plot.update(options)
    }
  },

  mounted () {
    // 初始化绘图
    const geometry = this.data.geometry
    const options = this.computeOptions(this.data)
    const plot = new getPlot[geometry](this.$refs.chart, options)
    this.plot = plot
    plot.render()
    this.handleResize(this.$refs.chart.getBoundingClientRect())
  },

  beforeDestroy () {
    // 销毁组件的时候销毁绘图
    if (this.plot) {
      this.destroy()
    }
  },

  methods: {
    computeOptions (originalOptions) {
      const geometry = originalOptions.geometry
      if (geometry === 'Line') {
        return this.getLineOptions(originalOptions)
      } else if (geometry === 'DualAxes') {
        // 需要转化内部的 geometryOptions
        const { geometryOptions } = originalOptions
        if (geometryOptions[0].geometry === 'Line') {
          geometryOptions[0] = this.getLineOptions(geometryOptions[0])
        } else {
          geometryOptions[0].geometry = 'column'
        }
        if (geometryOptions[1].geometry === 'Line') {
          geometryOptions[1] = this.getLineOptions(geometryOptions[0])
        } else {
          geometryOptions[1].geometry = 'column'
        }
      }
      return originalOptions
    },

    getLineOptions (originalOptions) {
      const {
        lineStyleMap,
        useLineStyle,
        pointMap,
        usePoint,
        ...options
      } = originalOptions
      useLineStyle && (options.lineStyle = function ({ category }) {
        return lineStyleMap[category]
      })
      usePoint && (options.point = function ({ category }) {
        return pointMap[category]
      })
      options.geometry = options.geometry.toLowerCase()
      return options
    },

    // 销毁绘画
    destroy () {
      this.plot.destroy()
      this.plot = null
    },

    handleResize ({ width, height }) {
      const that = this
      setTimeout(() => {
        that.plot && that.plot.update()
      }, 100)
      this.plot && this.plot.changeSize(width, height - 20)
    }
  }
}
</script>
