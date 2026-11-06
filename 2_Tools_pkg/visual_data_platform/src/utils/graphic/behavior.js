/** 定义交互
 * @author xunger
 * @time 2019-12-5
 */
import G6 from '@antv/g6'

const { message } = require('ant-design-vue')

// 增加边的动作
G6.registerBehavior('add-edge', {
  // 边的默认配置
  getDefaultCfg () {
    return {
      // 是否可以连线判定
      canLink: (e) => {
        // 连线的要求是目标为锚点
        return e.target.get('className') === 'Anchor'
      }
    }
  },
  getEvents () {
    // 得到相关的事件
    // 包括节点的鼠标点击、鼠标提起、鼠标移动
    return {
      'node:mousedown': 'onMousedown',
      'mouseup': 'onMouseup',
      'mousemove': 'onMousemove'
    }
  },
  // 鼠标点击触发事件
  onMousedown (ev) {
    // 判断是否可以连线，不行就返回
    if (!this.canLink(ev)) {
      return
    }
    // 节点
    const node = ev.item
    const graph = this.graph
    // 鼠标点击点
    const point = { x: ev.x, y: ev.y }
    // 获取对应点击的模型
    const model = node.getModel()
    // 如果在添加边的过程中，再次点击另一个节点，结束边的添加
    if (!this.addingEdge || !this.edge) {
      // 点击节点，触发增加边
      const nearestAnchor = node.getLinkPoint(point)

      // 如果是输入锚点，不予理睬
      if (nearestAnchor.anchorIndex < model.inAnchor.length) {
        return
      }
      this.startAnchor = model.outAnchor[nearestAnchor.anchorIndex - model.inAnchor.length]

      // 增加动态显示的边
      this.edge = graph.addItem('edge', {
        source: model.id,
        sourceAnchor: nearestAnchor.anchorIndex,
        target: point
      }, false)
      // 表明现在正在增加节点
      this.addingEdge = true
    }
  },
  // 鼠标释放的时候触发
  onMouseup (ev) {
    // 如果不是在连线，就返回
    if (!this.addingEdge || !this.edge) return
    // 得到鼠标释放时候的节点
    const node = ev.item
    // 鼠标点击位置
    const point = { x: ev.x, y: ev.y }
    const graph = this.graph
    // 释放位置节点的模型
    const model = node.getModel()

    // 判断终点是否是起点
    if (node.getType() === 'edge' || model.id === this.edge.getSource().getModel().id) {
      // 如果自己连自己就取消连线
      graph.removeItem(this.edge, false)
      // 取消状态
      this.edge = null
      this.addingEdge = false
    } else if (node.getType() === 'node') {
      // 判断最近的锚点
      const nearestAnchor = node.getLinkPoint(point)

      // 如果连接到输入点
      if (nearestAnchor.anchorIndex >= model.inAnchor.length) {
        // 取消连线
        graph.removeItem(this.edge, false)
        // 清除状态
        this.edge = null
        this.addingEdge = false
      }
      this.endAnchor = model.inAnchor[nearestAnchor.anchorIndex]
      // 判断类型是否一致
      let pass = false
      if (this.startAnchor.type.toLowerCase() === 'any') {
        pass = true
      } else if (this.endAnchor.type.toLowerCase() === 'any') {
        pass = true
      } else if (this.startAnchor.type.toLowerCase() === this.endAnchor.type.toLowerCase()) {
        pass = true
      }

      if (!pass) {
        // 取消连线
        graph.removeItem(this.edge, false)
        // 清除状态
        this.edge = null
        this.addingEdge = false
        message.error('锚点数据类型不一致，不能连接！')
      }

      // 查看目标节点对应的连线是否唯一
      const edges = node.getInEdges()
      for (const edge of edges) {
        // 如果存在多条连入，删除其余
        if (edge.getModel().targetAnchor === nearestAnchor.anchorIndex) {
          graph.removeItem(edge)
        }
      }
      // 更新连线的终点位置
      const addModal = this.edge.getModel()
      graph.removeItem(this.edge, false)
      graph.addItem('edge', {
        ...addModal,
        target: model.id,
        targetAnchor: nearestAnchor.anchorIndex
      })
      // 清除状态
      this.edge = null
      this.addingEdge = false
    }
  },
  // 鼠标移动触发
  onMousemove (ev) {
    // 鼠标移动过程中线隧动
    const point = { x: ev.x, y: ev.y }
    if (this.addingEdge && this.edge) {
      // 增加边的过程中，移动时边跟着移动
      this.graph.updateItem(this.edge, {
        target: point
      }, false)
    }
  }
})

// 鼠标悬浮边的时候样式
G6.registerBehavior('active-edge', {
  getEvents () {
    /** 激活边的事件集合
     * 鼠标进入时候设置状态
     * 鼠标移开的时候清除状态
     */
    return {
      'edge:mouseenter': 'setAllItemStates',
      'edge:mouseleave': 'clearAllItemStates'
    }
  },
  shouldBegin (e) {
    // 拖拽过程中没有目标节点，只有 x, y 坐标，不点亮
    const edge = e.item
    if (edge.getTarget().x) return false
    return true
  },
  // 设置激活状态
  setAllItemStates (e) {
    // 首先判断是否是当前事件
    if (!this.shouldBegin(e)) return
    // 1.激活当前选中的边
    const { graph } = this
    const edge = e.item
    // 设置边的状态
    graph.setItemState(edge, 'highlight', true)

    // 2. 激活边关联的 sourceNode 与 targetNode
    graph.setItemState(edge.getTarget(), 'active', true)
    graph.setItemState(edge.getSource(), 'active', true)
  },
  // 清除激活状态
  clearAllItemStates (e) {
    // 判断是否可以进行
    if (!this.shouldBegin(e)) return
    // 状态还原
    const { graph } = this
    const edge = e.item
    // 清除高亮边
    graph.setItemState(edge, 'highlight', false)
    // 清除前后节点的激活状态
    graph.setItemState(edge.getTarget(), 'active', false)
    graph.setItemState(edge.getSource(), 'active', false)
  }
})
