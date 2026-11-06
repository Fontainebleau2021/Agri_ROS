const {
  resetAllState,
  setNodeState,
  setNodeProgress
} = require('@/utils/graphic')

const { PythonShell } = window.require('python-shell')

const vuex = require('@/vuex')
const moment = require('moment')
const { getDagOrder } = require('./dag')
const broadcastStateChange = vuex.broadcastStateChange

const pyState = {
  running: false,
  pyShell: null,
  lastNode: null
}

const run = function (pythonScript, value) {
  if (pyState.running) {
    vuex.error('正在执行其它计算任务，请先终止计算!')
    return
  }

  vuex.graph.setMode('locked')
  vuex.running = true

  // 启动脚本
  pyState.pyShell = new PythonShell(pythonScript, {
    pythonPath: vuex.store.get('VUE_APP_PYTHON'),
    scriptPath: vuex.store.get('VUE_APP_MODEL_PATH')
  })

  pyState.running = true

  // 发送数据
  pyState.pyShell.send(JSON.stringify(value))

  // 收消息
  pyState.pyShell.on('message', function (message) {
    if (message.startsWith('$node-state$')) {
      // 更新的节点状态
      const msg = message.substring(13)
      const value = JSON.parse(msg)

      if (pyState.lastNode && pyState.lastNode !== value.nodeId) {
        setNodeState(pyState.lastNode, 'runState', false)
      }
      pyState.lastNode = value.nodeId
      broadcastStateChange(value.nodeId, value.state, value.value)
      setNodeState(value.nodeId, value.state, value.value)
    } else if (message.startsWith('$error$')) {
      const msg = message.substring(8)
      vuex.error(msg)
    } else if (message.startsWith('$node-progress$')) {
      const msg = message.substring(16)
      const value = JSON.parse(msg)
      setNodeProgress(value.nodeId, value.value)
    } else if (message.includes('$node-state$')) {
      const index = message.indexOf('$node-state$')
      const msg = message.substring(13 + index)
      const value = JSON.parse(msg)
      if (pyState.lastNode && pyState.lastNode !== value.nodeId) {
        setNodeState(pyState.lastNode, 'runState', false)
      }
      pyState.lastNode = value.nodeId
      broadcastStateChange(value.nodeId, value.state, value.value)
      setNodeState(value.nodeId, value.state, value.value)
    } else if (message.includes('$node-progress$')) {
      const index = message.indexOf('$node-progress$')
      const msg = message.substring(16 + index)
      const value = JSON.parse(msg)
      setNodeProgress(value.nodeId, value.value)
    } else {
      vuex.log(message)
    }
  })

  pyState.pyShell.end(function (err, code, signal) {
    if (pyState.lastNode) {
      setNodeState(pyState.lastNode, 'runState', false)
    }
    vuex.running = false
    pyState.running = false
    if (err) {
      vuex.error(err.stack)
      vuex.error('')
    } else {
      vuex.finish('执行结束(画布解锁)!')
      vuex.finish('')
    }
    vuex.graph.setMode('default')
  })
}

const runNode = function (node, appId, mode = 'train') {
  vuex.resetTerm && vuex.resetTerm()
  // 获取入边
  const edges = node.getInEdges()

  // 先默认填充
  const nodeModel = node.getModel()
  const inStore = nodeModel.inAnchor.map(() => { return null })

  // 按照位置插入数据
  for (const edge of edges) {
    const edgeModel = edge.getModel()
    const inNode = edge.getSource().getModel()
    inStore[edgeModel.targetAnchor] = {
      file: `${appId}_${edgeModel.source}`,
      key: edgeModel.sourceAnchor - inNode.inAnchor.length
    }
  }

  vuex.success(`$ 执行节点(锁定画布) - ${nodeModel.label} ${moment().format('YYYY-MM-DD HH:mm:ss')}`)
  run('runNode.py', {
    appId,
    inStore,
    mode: mode,
    model: nodeModel
  })
}

const runGraph = function (nodes, appId, timer, mode = 'train', part = false) {
  vuex.resetTerm && vuex.resetTerm()
  resetAllState()
  const nodePreMap = {}
  const nodeModelMap = {}
  const inStoreMap = {}
  const ids = []

  for (const node of nodes) {
    // 获取入边
    const edges = node.getInEdges()

    // 先默认填充
    const nodeModel = node.getModel()
    const inStore = nodeModel.inAnchor.map(() => { return null })
    ids.push(nodeModel.id)

    nodePreMap[nodeModel.id] = []

    // 按照位置插入数据
    for (const edge of edges) {
      const edgeModel = edge.getModel()
      const inNode = edge.getSource().getModel()
      inStore[edgeModel.targetAnchor] = {
        id: edgeModel.source,
        key: edgeModel.sourceAnchor - inNode.inAnchor.length,
        file: `${appId}_${edgeModel.source}`
      }
      nodePreMap[nodeModel.id].push(edgeModel.source)
    }
    inStoreMap[nodeModel.id] = inStore
    nodeModelMap[nodeModel.id] = nodeModel
  }

  // 获取执行顺序
  const runOrder = getDagOrder(nodePreMap)

  vuex.success(`$ 执行图 ${moment().format('YYYY-MM-DD HH:mm:ss')}`)
  run('runGraph.py', {
    appId,
    mode: mode,
    timer: timer,
    runOrder: runOrder,
    nodeModelMap: nodeModelMap,
    inStoreMap: inStoreMap,
    ids: ids,
    part: part
  })
}

const getRunConfig = function (nodes, appId, timer, mode = 'train', part = false) {
  const nodePreMap = {}
  const nodeModelMap = {}
  const inStoreMap = {}
  const ids = []

  for (const node of nodes) {
    // 获取入边
    const edges = node.getInEdges()

    // 先默认填充
    const nodeModel = node.getModel()
    const inStore = nodeModel.inAnchor.map(() => { return null })
    ids.push(nodeModel.id)

    nodePreMap[nodeModel.id] = []

    // 按照位置插入数据
    for (const edge of edges) {
      const edgeModel = edge.getModel()
      const inNode = edge.getSource().getModel()
      inStore[edgeModel.targetAnchor] = {
        id: edgeModel.source,
        key: edgeModel.sourceAnchor - inNode.inAnchor.length,
        file: `${appId}_${edgeModel.source}`
      }
      nodePreMap[nodeModel.id].push(edgeModel.source)
    }
    inStoreMap[nodeModel.id] = inStore
    nodeModelMap[nodeModel.id] = nodeModel
  }

  // 获取执行顺序
  const runOrder = getDagOrder(nodePreMap)
  return {
    appId,
    mode: mode,
    timer: timer,
    runOrder: runOrder,
    nodeModelMap: nodeModelMap,
    inStoreMap: inStoreMap,
    ids: ids,
    part: part
  }
}

const stopGraph = function () {
  vuex.running = false
  pyState.running = false
  pyState.pyShell.kill()
  pyState.pyShell = null
  vuex.success(`$ 停止计算`)
  if (pyState.lastNode) {
    broadcastStateChange(pyState.lastNode, 'runState', false)
    setNodeState(pyState.lastNode, 'runState', false)
  }
}

const generateCode = function (folder, data) {
  const path = window.require('path')
  const modelFolder = path.normalize(path.resolve(vuex.store.get('VUE_APP_MODEL_PATH')))
  run('generate.py', {
    content: data,
    modelPath: modelFolder,
    targetPath: folder
  })
}

module.exports = {
  runNode,
  runGraph,
  stopGraph,
  getRunConfig,
  generateCode
}
