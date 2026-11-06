/** 定义节点
 * @author xunger
 * @time 2019-12-5
 */
import G6 from '@antv/g6'

const formatInt = (num, mult = 5, ceil = true) => {
  return ceil
    ? Math.ceil(num / mult) * mult
    : Math.floor(num / mult) * mult
}

// 基础节点
G6.registerNode('node-base', {
  draw (cfg, group) {
    // 主元素
    const shape = group.addShape('rect', {
      // 长方形
      attrs: {
        x: 0,
        y: 0,
        width: cfg.width,
        height: cfg.height + 30,
        // 背景色
        stroke: '#DADADA',
        // 加一点倒角
        radius: [10, 10, 10, 10],
        fill: '#DADADA'
      }
    })
    // 定义标题背景
    group.addShape('rect', {
      attrs: {
        x: 0,
        y: 0,
        width: cfg.width,
        height: 30,
        stroke: '#aaa',
        // 上面加倒角
        radius: [ 10, 10, 0, 0],
        fill: '#aaa'
      },
      draggable: true
    })
    // 运行状态
    group.addShape('circle', {
      // 默认为灰色，表示没有运行
      attrs: {
        x: 10,
        y: 15,
        r: 5,
        fill: 'grey'
      },
      draggable: true
    })
    // 定义标题
    group.addShape('text', {
      attrs: {
        x: 25,
        y: 5,
        // 加入标题
        text: cfg.label,
        // 黄绿色
        fill: 'black',
        // 字体大小
        fontSize: 16,
        textBaseline: 'top'
      },
      draggable: true
    })

    group.addShape('text', {
      attrs: {
        x: cfg.width - 15,
        y: 13,
        fontFamily: 'iconfont',
        textAlign: 'center',
        textBaseline: 'middle',
        text: '\ue851',
        fontSize: 20,
        // 黄绿色
        fill: '#EEEEEE',
        cursor: 'pointer'
      },
      // must be assigned in G6 3.3 and later versions. it can be any value you want
      name: 'play'
    })
    // 返回定义的形状
    return shape
  },
  afterDraw (cfg, group) {
    // 当主元素绘制完成的时候绘制锚点
    this.drawAnchor(cfg, group)
  },
  getAnchorPoints (cfg) {
    const inAnchor = cfg.inAnchor || []
    const outAnchor = cfg.outAnchor || []
    const split = Math.max(inAnchor.length, outAnchor.length)
    const anchors = []
    inAnchor.forEach((anchor, index) => {
      anchors.push([0, formatInt(cfg.height * (index + 1) / (split + 1) + 30) / (cfg.height + 30), { flow: 'in', ...anchor }])
    })
    outAnchor.forEach((anchor, index) => {
      anchors.push([1, formatInt(cfg.height * (index + 1) / (split + 1) + 30) / (cfg.height + 30), { flow: 'out', ...anchor }])
    })
    return anchors
  },
  drawAnchor (cfg, group) {
    const inAnchor = cfg.inAnchor || []
    const outAnchor = cfg.outAnchor || []
    const split = Math.max(inAnchor.length, outAnchor.length)

    // 绘制入锚点
    inAnchor.forEach((anchor, index) => {
      group.addShape('rect', {
        className: 'Anchor',
        attrs: {
          // 确定锚点位置
          x: 0,
          y: formatInt(cfg.height * (index + 1) / (split + 1)) + 25,
          width: 6,
          height: 10,
          fill: '#bbb',
          stroke: '#bbb',
          cursor: 'pointer'
        }
      })
      // 加入文本
      group.addShape('text', {
        attrs: {
          // 加入锚点文本，偏离锚点一定位置
          x: 10,
          y: formatInt(cfg.height * (index + 1) / (split + 1)) + 36,
          // 锚点文本
          text: anchor.label,
          fill: 'black',
          fontSize: 16,
          fontFamily: 'Tahoma',
          textAlign: 'left'
        },
        draggable: true
      })
    })

    outAnchor.forEach((anchor, index) => {
      group.addShape('rect', {
        className: 'Anchor',
        attrs: {
          // 确定锚点位置
          x: cfg.width - 6,
          y: formatInt(cfg.height * (index + 1) / (split + 1)) + 25,
          width: 6,
          height: 10,
          fill: '#bbb',
          stroke: '#bbb',
          cursor: 'pointer'
        }
      })
      // 加入文本
      group.addShape('text', {
        attrs: {
          // 加入锚点文本，偏离锚点一定位置
          x: cfg.width - 10,
          y: formatInt(cfg.height * (index + 1) / (split + 1)) + 36,
          // 锚点文本
          text: anchor.label,
          fill: 'black',
          fontSize: 16,
          fontFamily: 'Tahoma',
          textAlign: 'right'
        },
        draggable: true
      })
    })
  },
  // 设置状态
  setState (name, value, item) {
    // 得到绘制的形状集合
    const group = item.getContainer()
    const keyShape = item.get('keyShape')
    // 获取状态shape
    const shape = group.get('children')[2] // 顺序根据 draw 时确定
    const icon = group.get('children')[4]

    // 如果设置运行状态
    if (name === 'runState') {
      if (value) {
        // 运行时候填充色
        shape.attr('fill', '#EE9D51')
        icon.attr('text', '\ue774')
      } else {
        // 没有运行的时候填充
        icon.attr('text', '\ue851')
        shape.attr('fill', '#52EA34')
      }
    }

    // 恢复颜色
    if (name === 'reset') {
      item.clearStates('runState')
      // 设置reset的时候恢复原来颜色
      shape.attr('fill', 'grey')
    }

    if (name === 'selected') {
      if (value) {
        keyShape.attr('shadowBlur', 8)
        keyShape.attr('shadowColor', '#A25FCE')
      } else {
        keyShape.attr('shadowBlur', 0)
        keyShape.attr('shadowColor', 'transparent')
      }
    }
  }
})

G6.registerNode('node-chip', {
  draw (cfg, group) {
    const commonAttr = {
      x: -cfg.width / 2,
      y: -cfg.height / 2,
      width: cfg.width,
      height: cfg.height,
      // 加一点倒角
      radius: [10, 10, 10, 10],
      fill: cfg.color || '#FFDEB7'
    }

    // 主元素
    const shape = group.addShape('rect', {
      // 长方形
      attrs: commonAttr
    })
    // 第一个背景
    group.addShape('rect', {
      zIndex: -3,
      attrs: {
        ...commonAttr,
        opacity: 0.8
      },
      // must be assigned in G6 3.3 and later versions. it can be any value you want
      name: 'back-shape1'
    })
    // 第二个背景
    group.addShape('rect', {
      zIndex: -2,
      attrs: {
        ...commonAttr,
        opacity: 0.8
      },
      // must be assigned in G6 3.3 and later versions. it can be any value you want
      name: 'back-shape2'
    })
    // 第三个背景
    group.addShape('rect', {
      zIndex: -1,
      attrs: {
        ...commonAttr,
        opacity: 0.8
      },
      // must be assigned in G6 3.3 and later versions. it can be any value you want
      name: 'back-shape3'
    })
    group.sort()

    // bottom percent
    group.addShape('rect', {
      attrs: {
        x: -cfg.width / 2,
        y: cfg.height / 2 + 2,
        width: cfg.width,
        radius: [2, 2, 2, 2],
        height: 4,
        fill: '#DCDFE5'
      },
      draggable: true
    })

    if (cfg.progress) {
      group.addShape('rect', {
        attrs: {
          x: -cfg.width / 2,
          y: cfg.height / 2 + 2,
          width: cfg.width * (cfg.progress || 0) / 100,
          radius: [2, 2, 2, 2],
          height: 4,
          fill: '#62C400'
        },
        draggable: true
      })
    }

    // 文本
    group.addShape('text', {
      attrs: {
        text: cfg.label,
        x: 0,
        y: cfg.height / 2 + 16,
        fontSize: 14,
        textAlign: 'center',
        textBaseline: 'middle',
        fill: '#111'
      },
      name: 'text-shape'
    })
    const imgHeight = Math.min(cfg.width * 2 / 3, cfg.height * 2 / 3)
    // 文本
    group.addShape('image', {
      attrs: {
        x: -cfg.width / 3,
        y: -imgHeight / 2,
        width: cfg.width * 2 / 3,
        height: imgHeight,
        img: cfg.icon
      },
      // must be assigned in G6 3.3 and later versions. it can be any value you want
      name: 'image-shape',
      draggable: true
    })
    return shape
  },
  afterDraw (cfg, group) {
    // 当主元素绘制完成的时候绘制锚点
    this.drawAnchor(cfg, group)
  },
  getAnchorPoints (cfg) {
    const inAnchor = cfg.inAnchor || []
    const outAnchor = cfg.outAnchor || []
    const split = Math.max(inAnchor.length, outAnchor.length)
    const anchors = []
    inAnchor.forEach((anchor, index) => {
      anchors.push([0, formatInt(cfg.height * (index + 1) / (split + 1)) / cfg.height, { flow: 'in', ...anchor }])
    })
    outAnchor.forEach((anchor, index) => {
      anchors.push([1, formatInt(cfg.height * (index + 1) / (split + 1)) / cfg.height, { flow: 'out', ...anchor }])
    })
    return anchors
  },
  drawAnchor (cfg, group) {
    const inAnchor = cfg.inAnchor || []
    const outAnchor = cfg.outAnchor || []
    const split = Math.max(inAnchor.length, outAnchor.length)

    // 绘制入锚点
    inAnchor.forEach((anchor, index) => {
      group.addShape('marker', {
        className: 'Anchor',
        name: 'anchor',
        attrs: {
          // 确定锚点位置
          x: -cfg.width / 2,
          y: formatInt(cfg.height * (index + 1) / (split + 1)) - cfg.height / 2,
          r: 4,
          symbol: function (x, y, r) {
            return [
              ['M', x, y - r ], ['L', x + r, y - r], ['L', x + r, y + r], ['L', x, y + r], ['Z']]
          },
          fill: '#bbb',
          stroke: '#bbb',
          cursor: 'pointer'
        }
      })
    })

    outAnchor.forEach((anchor, index) => {
      group.addShape('marker', {
        className: 'Anchor',
        name: 'anchor',
        attrs: {
          // 确定锚点位置
          x: cfg.width / 2,
          y: formatInt(cfg.height * (index + 1) / (split + 1)) - cfg.height / 2,
          r: 4,
          symbol: function (x, y, r) {
            return [
              ['M', x - r, y - r], ['L', x, y - r ], ['L', x, y + r], ['L', x - r, y + r], ['Z']]
          },
          fill: '#bbb',
          stroke: '#bbb',
          cursor: 'pointer'
        }
      })
    })
  },
  // 设置状态
  setState (name, value, item) {
    // 得到绘制的形状集合
    const group = item.getContainer()
    const keyShape = item.get('keyShape')
    // 获取状态shape
    const children = group.get('children')
    const back1 = children[0]
    const back2 = children[1]
    const back3 = children[2]

    const cfg = item.getModel()
    const commonEndAttr = {
      x: -cfg.width / 2 - 10,
      y: -cfg.height / 2 - 10,
      width: cfg.width + 20,
      height: cfg.height + 20
    }
    const commonStartAttr = {
      x: -cfg.width / 2,
      y: -cfg.height / 2,
      width: cfg.width,
      height: cfg.height
    }

    // 如果设置运行状态
    if (name === 'runState') {
      if (value) {
        back1.animate({
          ...commonEndAttr,
          opacity: 0.1
        }, {
          repeat: true, // 循环
          duration: 900,
          easing: 'easeCubic',
          delay: 0 // 无延迟
        })
        back2.animate({
          ...commonEndAttr,
          opacity: 0.1
        }, {
          repeat: true, // 循环
          duration: 900,
          easing: 'easeCubic',
          delay: 300 // 1 秒延迟
        }) // 1 秒延迟
        back3.animate({
          ...commonEndAttr,
          opacity: 0.1
        }, {
          repeat: true, // 循环
          duration: 900,
          easing: 'easeCubic',
          delay: 600 // 1 秒延迟
        }) // 1 秒延迟
      } else {
        back1.stopAnimate()
        back2.stopAnimate()
        back3.stopAnimate()
        back1.attr(commonStartAttr)
        back2.attr(commonStartAttr)
        back3.attr(commonStartAttr)
      }
    }

    if (name === 'selected') {
      if (value) {
        keyShape.attr('shadowBlur', 8)
        keyShape.attr('shadowColor', '#E44D26')
      } else {
        keyShape.attr('shadowBlur', 0)
        keyShape.attr('shadowColor', 'transparent')
      }
    }
  }
})

// 基础节点
G6.registerNode('node-alert', {
  draw (cfg, group) {
    // 主元素
    const shape = group.addShape('circle', {
      // 默认为灰色，表示没有运行
      attrs: {
        x: 0,
        y: 0,
        r: (cfg.height + cfg.width) / 4,
        fill: 'grey'
      },
      draggable: true
    })

    // 返回定义的形状
    return shape
  },
  afterDraw (cfg, group) {
    // 当主元素绘制完成的时候绘制锚点
    this.drawAnchor(cfg, group)
  },
  getAnchorPoints (cfg) {
    const inAnchor = cfg.inAnchor || []
    const anchors = []
    anchors.push([0, 0.5, { flow: 'in', ...inAnchor[0] }])
    return anchors
  },
  drawAnchor (cfg, group) {
    group.addShape('rect', {
      className: 'Anchor',
      attrs: {
        // 确定锚点位置
        x: -(cfg.height + cfg.width) / 4,
        y: -5,
        width: 6,
        height: 10,
        fill: '#89FF6F',
        stroke: '#89FF6F',
        cursor: 'pointer'
      }
    })
  },
  // 设置状态
  setState (name, value, item) {
    const keyShape = item.get('keyShape')

    // 如果设置运行状态
    if (name === 'error') {
      if (value) {
        // 运行时候填充色
        keyShape.attr('fill', 'red')
      } else {
        // 没有运行的时候填充
        keyShape.attr('fill', 'green')
      }
    }

    // 恢复颜色
    if (name === 'reset') {
      // 设置reset的时候恢复原来颜色
      keyShape.attr('fill', 'grey')
    }
  }
})
