import G6 from '@antv/g6'

// 边的样式定义
const bizFlowEdge = {
  options: {
    // 边的默认样式
    style: {
      // 线条颜色
      stroke: '#ccc1d8',
      // 线条宽度
      lineWidth: 2,
      lineAppendWidth: 20,
      // 不要阴影
      shadowColor: null,
      shadowBlur: 0,
      // 倒角
      radius: 8,
      // 偏离
      offset: 24,
      // 结束的箭头样式
      endArrow: {
        path: 'M 0,0 L 4,3 L 4,-3 Z'
      }
    },
    // 状态的样式定义
    stateStyles: {
      // 选择的时候样式
      selected: {
        // 红色
        stroke: 'red',
        // 附加阴影
        shadowColor: '#5aaaff',
        shadowBlur: 24
      },
      // 高亮样式
      highlight: {
        // 绿色
        stroke: 'green',
        // 附加阴影
        shadowColor: '#5aaaff',
        shadowBlur: 24
      }
    }
  },
  // 设置边的状态
  setState (name, value, item) {
    const shape = item.get('keyShape')
    // 如果没有边，结束
    if (!shape) {
      return
    }

    // 得到默认演示
    const { style, stateStyles } = this.options
    // 根据状态名称获取对应样式
    const stateStyle = stateStyles[name]

    // 没有样式就无需设置
    if (!stateStyle) {
      return
    }

    // 如果状态为true就设置样式
    if (value) {
      // 融合新样式到基本样式
      shape.attr({
        ...style,
        ...stateStyle
      })
    } else {
      // 采用默认样式
      shape.attr(style)
    }
  }
}

// 注册边
G6.registerEdge('biz-flow-edge', bizFlowEdge, 'cubic-horizontal')
