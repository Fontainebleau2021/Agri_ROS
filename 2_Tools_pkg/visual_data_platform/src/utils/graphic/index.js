// 绘图模块
const G6 = require('@antv/g6')

const vuex = require('@/vuex')

const initGraphic = function (divId, graphicData) {
  // 网格实例
  const grid = new G6.Grid()

  const toolbar = new G6.ToolBar({
    className: 'hidden-element'
  })

  vuex.graph = undefined
  window.vuex = vuex
  vuex.toolbar = toolbar

  // 获取容器的高度和宽度
  const { clientHeight, clientWidth } = document.getElementById(divId)
  // 初始的配置
  const cfgs = {
    container: divId,
    // 高度和宽度
    width: clientWidth,
    height: clientHeight,
    // 交互的模式
    modes: {
      // 拖动节点、拖动画布、缩放画布
      default: {
        // 节点
        'drag-node': {
          type: 'drag-node',
          enableDelegate: false
        },
        // 画布拖拽
        'drag-canvas': 'drag-canvas',
        // 画布缩放
        'zoom-canvas': 'zoom-canvas',
        // 增加边
        'add-edge': 'add-edge',
        // 边的触发
        'active-edge': 'active-edge',
        'click-select': 'click-select',
        'brush-select': 'brush-select',
        'drag-combo': 'drag-combo',
        'collapse-expand-combo': 'collapse-expand-combo'
      },
      // 锁定画布的时候只允许边激活
      locked: {
        // 画布拖拽
        'drag-canvas': 'drag-canvas',
        // 画布缩放
        'zoom-canvas': 'zoom-canvas'
      }
    },
    // 默认的节点
    defaultNode: {
      type: 'node-chip'
    },
    // 默认的边
    defaultEdge: {
      type: 'biz-flow-edge'
    },
    defaultCombo: {
      // ... 其他属性
      stateStyles: {
        'selected': {
          stroke: '#E44D26'
        }
      },
      style: {
        stroke: '#C9E5FA'
      },
      labelCfg: {
        refY: 18,
        position: 'bottom'
      }
    },
    groupType: 'rect',
    groupStyle: {
      default: {},
      hover: {},
      collapse: {}
    }
  }

  // 建立图
  vuex.graph = new G6.Graph({
    plugins: [grid, toolbar],
    enabledStack: true,
    ...cfgs
  })

  // 加载并绘制数据
  vuex.graph.data(graphicData)
  vuex.graph.render()
}

const addItem = function (model, type = 'node', stack = true) {
  if (type === 'node' && !model.id) {
    vuex.nodeId++
    model.id = `node${vuex.nodeId}`
  }

  if (type === 'combo') {
    vuex.graph.createCombo({
      id: `combo${vuex.groupId}`,
      label: `组合${vuex.groupId}`
    }, model, stack)
    vuex.groupId++
  } else {
    vuex.graph.addItem(type, {
      ...model
    }, stack)
  }
}

const removeItem = function (model, stack = true) {
  vuex.graph.removeItem(model, stack)
}

const setNodeState = function (nodeId, state, value) {
  const node = vuex.graph.findById(nodeId)
  if (node !== undefined) {
    node.clearStates(state)
    node.setState(state, value)
    vuex.graph.paint()
  }
}

const setNodeProgress = function (nodeId, value) {
  const node = vuex.graph.findById(nodeId)
  if (node !== undefined) {
    node.update({
      progress: value
    })
    vuex.graph.paint()
  }
}

const setNodeError = function (nodeId, msg) {
  const node = vuex.graph.findById(nodeId)
  if (node !== undefined) {
    node.update({
      error: msg
    })
    vuex.graph.paint()
  }
}

const resetAllState = function () {
  const nodes = vuex.graph.getNodes()
  for (const node of nodes) {
    node.setState('reset', true)
  }
  vuex.graph.paint()
}

module.exports = {
  initGraphic,
  addItem,
  removeItem,
  setNodeState,
  setNodeProgress,
  setNodeError,
  resetAllState
}
