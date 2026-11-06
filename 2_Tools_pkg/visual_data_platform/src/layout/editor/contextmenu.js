import vuex from '@/vuex'

import { cloneDeep } from 'lodash'
import { runGraph } from '@/utils/core'
import { getSourceNodesRecursive, getTargetNodesRecursive } from '@/utils/core/dag'
import { addItem, removeItem, setNodeState } from '@/utils/graphic'
import { readFile, saveFile } from '@/utils/file'

const path = require('path')
const changeFilePathToRelative = function (model) {
  model.icon = path.relative('.', model.icon)
  model.pythonPath = path.relative('.', model.pythonPath)
  model.configPath = path.relative('.', model.configPath)
  model.helpDocument = path.relative('.', model.helpDocument)
  return model
}

const constructNodeMenu = function (event) {
  const model = this.selectedNode.getModel()
  return {
    items: [
      {
        label: '参数配置',
        icon: 'iconfont icon-data_configuration',
        divided: true,
        onClick: () => {
          const options = model.kwargsModelSettings || {}
          if (model.kwargsModalType === 'imageCrop') {
            this.$ImageCrop({
              node: this.selectedNode,
              modalOptions: {
                headerTitle: model.label,
                headerLogo: `<img
                    src="${model.icon}"
                    style="margin-left:5px"
                    width="20"
                    height="20"
                  >`,
                dragit: {
                  containment: [0, -2000, -2000, -2000]
                },
                headerControls: 'closeonly',
                ...options
              }
            })
          } else if (model.kwargsModalType === 'imageLabelTool') {
            this.$ImageLabelTool({
              node: this.selectedNode,
              modalOptions: {
                headerTitle: model.label,
                headerLogo: `<img
                    src="${model.icon}"
                    style="margin-left:5px"
                    width="20"
                    height="20"
                  >`,
                dragit: {
                  containment: [0, -2000, -2000, -2000]
                },
                headerControls: 'closeonly',
                ...options
              }
            })
          } else if (model.kwargsModalType === 'excelEditor') {
            this.$ExcelEditor({
              node: this.selectedNode,
              modalOptions: {
                headerTitle: model.label,
                headerLogo: `<img
                    src="${model.icon}"
                    style="margin-left:5px"
                    width="20"
                    height="20"
                  >`,
                dragit: {
                  containment: [0, -2000, -2000, -2000]
                },
                headerControls: 'closeonly',
                ...options
              }
            })
          } else if (model.kwargsModalType === 'pyScript') {
            this.$PyScriptModal({
              node: this.selectedNode,
              modalOptions: {
                headerTitle: model.label,
                headerLogo: `<img
                  src="${model.icon}"
                  style="margin-left:5px"
                  width="20"
                  height="20"
                >`,
                dragit: {
                  containment: [0, -2000, -2000, -2000]
                },
                headerControls: 'closeonly',
                ...options
              }
            })
          } else {
            this.$JsonSchemaModal({
              node: this.selectedNode,
              modalOptions: {
                headerTitle: model.label,
                headerLogo: `<img
                  src="${model.icon}"
                  style="margin-left:5px"
                  width="20"
                  height="20"
                >`,
                contentSize: {
                  height: '400px',
                  width: '260px'
                },
                dragit: {
                  containment: [0, -2000, -2000, -2000]
                },
                resizeit: {
                  handles: 'n, e, s, w, ne, se, sw, nw',
                  minWidth: 260,
                  minHeight: 260
                },
                headerControls: 'closeonly',
                ...options
              }
            })
          }
        }
      },
      {
        label: '查看结果',
        icon: 'iconfont icon-result',
        disabled: !(model.progress === 100 || vuex.env !== 'development') || (model.outputVisualization || []).length === 0,
        divided: true,
        onClick: () => {
          this.$NodeVisualization({
            visualizationOptions: this.selectedNode.getModel().outputVisualization || [],
            nodeId: model.id,
            nodeLabel: model.label
          })
        }
      },
      {
        label: '运行到当前节点',
        icon: 'iconfont icon-play',
        onClick: () => {
          const timer = { ...vuex.timer }
          timer.enabled = false

          runGraph(
            Array.from(getSourceNodesRecursive(this.selectedNode)),
            vuex.appId,
            timer,
            vuex.env === 'development' ? 'train' : 'prod',
            true)
        }
      },
      {
        label: '运行后续节点',
        icon: 'iconfont icon-play',
        divided: true,
        onClick: () => {
          const timer = { ...vuex.timer }
          timer.enabled = false

          runGraph(
            Array.from(getTargetNodesRecursive(this.selectedNode)),
            vuex.appId,
            timer,
            vuex.env === 'development' ? 'train' : 'prod',
            true)
        }
      },
      {
        label: '重命名',
        icon: 'iconfont icon-rename',
        onClick: () => {
          this.widgetName = model.label
          this.widgetNameModalVisible = true
        }
      },
      {
        label: '复制',
        disabled: this.running,
        icon: 'iconfont icon-copy',
        onClick: () => {
          const nodes = vuex.graph.findAllByState('node', 'selected')
          const edges = vuex.graph.findAllByState('edge', 'selected')
          if (!nodes.includes(this.selectedNode)) {
            nodes.push(this.selectedNode)
          }

          let minX = 9999999999
          let minY = 9999999999
          for (const node of nodes) {
            const model = node.getModel()
            minX = Math.min(minX, model.x)
            minY = Math.min(minY, model.y)
          }

          vuex.copedItem = {
            nodes, edges, x: minX, y: minY
          }
        }
      },
      {
        label: '收藏',
        icon: 'iconfont icon-copy',
        divided: true,
        onClick: () => {
          const nodes = [this.selectedNode]
          const models = []

          let minX = 9999999999
          let minY = 9999999999
          for (const node of nodes) {
            const model = node.getModel()
            models.push(changeFilePathToRelative(model))
            minX = Math.min(minX, model.x)
            minY = Math.min(minY, model.y)
          }

          const template = JSON.parse(JSON.stringify(vuex.collect.template))
          template.default.data = {
            nodes: models, edges: [], x: minX, y: minY
          }
          template.default.label = this.selectedNode.getModel().label
          template.default.moduleKey = this.uuidv4()
          vuex.collect.components.push(template)
          saveFile(vuex.collect.file, 'module.exports = ' + JSON.stringify(vuex.collect.components, null, 2))
        }
      },
      {
        label: '分组',
        icon: 'iconfont icon-copy',
        divided: true,
        onClick: () => {
          const nodes = vuex.graph.findAllByState('node', 'selected')
          if (this.selectedNode && !nodes.includes(this.selectedNode)) {
            nodes.push(this.selectedNode)
          }
          const nodeIds = nodes.map(p => p.getModel().id)
          addItem([...nodeIds], 'combo')
        }
      },
      {
        label: '删除',
        disabled: this.running,
        icon: 'iconfont icon-delete',
        onClick: () => {
          const before = vuex.graph.save()
          const nodes = vuex.graph.findAllByState('node', 'selected')
          for (const node of nodes) {
            removeItem(node, false)
          }
          if (this.selectedNode && !this.selectedNode.destroyed) {
            removeItem(this.selectedNode, false)
          }
          const after = vuex.graph.save()
          vuex.graph.pushStack('changedata', {
            before, after
          }, 'undo')
        },
        divided: true
      },
      {
        label: '查看',
        disabled: this.running || model.virtual || model.kwargsModalType === 'pyScript',
        icon: 'iconfont icon-view',
        children: [
          { label: '代码', onClick: () => {
            this.codeVisible = false
            this.codeLang = 'python'
            this.codeContent = ''
            this.editModal = model
            readFile(model.pythonPath).then(content => {
              this.codeContent = content
              this.codeContentOld = content
              this.codeVisible = true
            })
          } },
          { label: '配置', onClick: () => {
            this.codeVisible = false
            this.codeLang = 'javascript'
            this.codeContent = ''
            this.editModal = model
            readFile(model.configPath).then(content => {
              this.codeContent = content
              this.codeContentOld = content
              this.codeVisible = true
            })
          } }
        ],
        divided: true
      },
      {
        icon: 'iconfont icon-document',
        disabled: !model.helpDocument,
        label: '帮助', onClick: () => {
          if (!model.helpDocument) return
          this.helpDocument = model.helpDocument
          this.documentVisible = true
        }
      }
    ],
    event,
    zIndex: 99999,
    minWidth: 200
  }
}

const constructEdgeMenu = function (event) {
  if (this.running) return

  return {
    items: [
      {
        label: '删除连线',
        icon: 'iconfont icon-delete',
        onClick: () => {
          removeItem(this.clickEvent.edge)
        }
      }
    ],
    event,
    zIndex: 99999,
    minWidth: 200
  }
}

const constructComboMenu = function (event) {
  const { combos } = vuex.graph.getComboChildren(this.clickEvent.combo)

  return {
    items: [
      {
        label: '收起|展开组合',
        icon: 'iconfont icon-delete',
        divided: true,
        onClick: () => {
          vuex.graph.collapseExpandCombo(this.clickEvent.combo)
        }
      },
      {
        label: '重命名',
        icon: 'iconfont icon-rename',
        onClick: () => {
          this.comboName = this.clickEvent.combo.getModel().label
          this.comboNameModalVisible = true
        }
      },
      {
        label: '拆分',
        icon: 'iconfont icon-delete',
        onClick: () => {
          vuex.graph.expandCombo(this.clickEvent.combo)
          vuex.graph.uncombo(this.clickEvent.combo)
        }
      },
      {
        label: '复制',
        icon: 'iconfont icon-copy',
        disabled: combos.length > 0,
        divided: true,
        onClick: () => {
          const { nodes } = vuex.graph.getComboChildren(this.clickEvent.combo)
          const edges = []

          const distinctNodes = []
          const ids = []
          let minX = 9999999999
          let minY = 9999999999
          for (const node of nodes) {
            const model = node.getModel()
            if (ids.includes(model.id)) {
              continue
            }
            ids.push(model.id)
            edges.push(...node.getEdges())
            distinctNodes.push(node)
            minX = Math.min(minX, model.x)
            minY = Math.min(minY, model.y)
          }

          vuex.copedItem = {
            nodes: distinctNodes, edges, combos, x: minX, y: minY, isCombo: true
          }
        }
      },
      {
        label: '收藏',
        icon: 'iconfont icon-copy',
        divided: true,
        onClick: () => {
          const { nodes } = vuex.graph.getComboChildren(this.clickEvent.combo)
          const edges = []

          const distinctNodes = []
          const nodeModels = []
          const edgeModels = []

          const ids = []
          let minX = 9999999999
          let minY = 9999999999
          for (const node of nodes) {
            const model = node.getModel()
            if (ids.includes(model.id)) {
              continue
            }
            nodeModels.push(changeFilePathToRelative(model))
            ids.push(model.id)

            for (const edge of node.getEdges()) {
              edges.push(edge)
              edgeModels.push(edge.getModel())
            }
            distinctNodes.push(node)
            minX = Math.min(minX, model.x)
            minY = Math.min(minY, model.y)
          }

          const template = JSON.parse(JSON.stringify(vuex.collect.template))
          template.default.data = {
            nodes: nodeModels, edges: edgeModels, combos, x: minX, y: minY, isCombo: true
          }
          template.default.moduleKey = this.uuidv4()
          template.default.label = this.clickEvent.combo.getModel().label
          vuex.collect.components.push(template)
          saveFile(vuex.collect.file, 'module.exports = ' + JSON.stringify(vuex.collect.components, null, 2))
        }
      },
      {
        label: '删除',
        icon: 'iconfont icon-delete',
        onClick: () => {
          removeItem(this.clickEvent.combo)
        }
      }
    ],
    event,
    zIndex: 99999,
    minWidth: 200
  }
}

const constructCanvasMenu = function (event) {
  if (this.running) return

  return {
    items: [
      {
        label: '粘贴',
        divided: true,
        disabled: vuex.copedItem === null,
        onClick: () => {
          const { nodes, edges, x, y, isCombo } = vuex.copedItem
          const np = vuex.graph.getPointByClient(event.clientX, event.clientY)

          const DeltaX = np.x - x
          const DeltaY = np.y - y

          const IdMap = {}
          const nodeIds = []
          const before = vuex.graph.save()

          for (const node of nodes) {
            node.setState('selected', false)
            const nodeCfg = cloneDeep(node.getModel())
            const key = nodeCfg.id
            // 其中配置是公共的，直接用同一个
            delete nodeCfg['id']
            nodeCfg['progress'] = 0

            // 寻找绘制坐标
            nodeCfg.x += DeltaX
            nodeCfg.y += DeltaY

            // 增加节点
            addItem(nodeCfg, 'node', false)
            IdMap[key] = nodeCfg.id
            nodeIds.push(nodeCfg.id)
            setNodeState(nodeCfg.id, 'selected', true)
          }

          for (const edge of edges) {
            const edgeConfig = cloneDeep(edge.getModel())

            delete edgeConfig['id']
            // 寻找绘制坐标
            edgeConfig.source = IdMap[edgeConfig.source]
            edgeConfig.target = IdMap[edgeConfig.target]

            if (edgeConfig.source && edgeConfig.target) {
              // 增加节点
              addItem(edgeConfig, 'edge', false)
            }
          }

          const after = vuex.graph.save()
          vuex.graph.pushStack('changedata', {
            before, after
          }, 'undo')

          vuex.copedItem = null
          if (isCombo) {
            addItem(nodeIds, 'combo')
          }
        }
      },
      {
        label: '原始缩放比例',
        onClick: () => {
          vuex.graph.zoomTo(1)
        }
      },
      {
        label: '缩放到合适',
        divided: true,
        onClick: () => {
          vuex.graph.fitView([20, 20])
        }
      },
      {
        label: '清空',
        onClick: () => {
          // 确定清空
          this.$confirm({
            title: '确定清空画布, 该操作无法撤销',
            okText: '清空',
            centered: true,
            okType: 'danger',
            cancelText: '取消',
            onOk () {
              vuex.graph.clear()
              // 画布更新
              vuex.graph.paint()
              vuex.graph.clearStack()
            }
          })
        }
      }
    ],
    event,
    zIndex: 99999,
    minWidth: 200
  }
}

export default {
  constructNodeMenu,
  constructEdgeMenu,
  constructComboMenu,
  constructCanvasMenu
}
