<template>
  <div class="layout">
    <div class="header">
      <x-header :header="vuex.info.name" />
    </div>
    <div class="body">
      <div class="siderbar">
        <xSiderbar
          :widgets="widgets"
          :selected-node="selectedNode"
        />
      </div>
      <div
        ref="CanvasNode"
        class="editor"
      >
        <drop
          class="fill-parent"
          @drop="handleDrop"
        >
          <xEditor
            ref="editor"
            :click-event="clickEvt"
            :selected-node="selectedNode"
          />
        </drop>
        <a-badge
          class="terminal-icon"
          :number-style="{ backgroundColor: vuex.termState }"
          :count="vuex.termCount"
        >
          <a-icon
            type="code"
            @click="termVisible=!termVisible;"
          />
        </a-badge>

        <!-- 缩放监听 -->
        <resize-observer @notify="handleResize" />
      </div>
    </div>
    <div class="footer">
      <xStatusbar />
    </div>

    <JSPanel
      :visible.sync="termVisible"
      :keep-dom="true"
      modal-id="terminalModal"
      :options="options"
      @resize="resizeTerm"
      @afterMounted="initTerm"
      @close="termVisible = false"
    >
      <div
        id="terminal"
        style="width: 100%; height: 100%; overflow: hidden;"
        @contextmenu="copySelected"
      />
      <div slot="footerToolbar">
        <a-icon
          type="stop"
          style="margin-right: 10px; cursor: pointer;"
          @click="vuex.clearTerm"
        />
        <a-select
          default-value="TomorrowNight"
          style="width: 180px"
          @change="handleChange"
        >
          <a-select-option
            v-for="item in themeOptions"
            :key="item.key"
            :value="item.key"
          >
            {{ item.label }}
          </a-select-option>
        </a-select>
      </div>
    </JSPanel>
  </div>
</template>

<script>
import xHeader from './header'
import xSiderbar from './siderbar'
import xStatusbar from './statusbar'
import xEditor from './editor'
import JSPanel from '@/components/jsPanel'
import xtermTheme from 'xterm-theme'

import { cloneDeep } from 'lodash'

const Terminal = window.require('xterm').Terminal
const ipcRenderer = window.require('electron').ipcRenderer
const { FitAddon } = window.require('xterm-addon-fit')

import vuex from '@/vuex'
import { initGraphic, removeItem, addItem, setNodeState } from '@/utils/graphic'

export default {
  components: {
    JSPanel,
    xHeader,
    xSiderbar,
    xEditor,
    xStatusbar
  },

  data () {
    return {
      widgets: [],
      clickEvt: {
        type: 'canvas',
        clickEventX: -400,
        clickEventY: -400
      },
      vuex: vuex,
      selectedNode: null,
      themeOptions: [
        { key: 'Violet_Light', label: 'Violet' },
        { key: 'AdventureTime', label: 'AdventureTime' },
        { key: 'DarkSide', label: 'DarkSide' },
        { key: 'Galaxy', label: 'Galaxy' },
        { key: 'Neutron', label: 'Neutron' },
        { key: 'Tomorrow_Night', label: 'TomorrowNight' },
        { key: 'idleToes', label: 'IdleToes' }
      ],
      termVisible: false,
      options: {
        headerTitle: '日志',
        theme: 'primary',
        contentSize: '800 400'
      }
    }
  },

  watch: {
    termVisible (isVisible) {
      if (isVisible) {
        vuex.term.scrollToBottom()
      }
    }
  },

  mounted () {
    // 初始化
    vuex.env = 'development'
    vuex.saveModelPath = null
    vuex.graphData = undefined
    vuex.appId = this.uuidv4()
    vuex.info = {
      name: '未命名',
      description: ''
    }

    initGraphic('editor-container', vuex.graphData || {
      nodes: [],
      edges: []
    })

    // 禁止默认的右键菜单
    document.oncontextmenu = function () {
      return false
    }

    this.attachEventToGraph()
    this.fetchModels()

    ipcRenderer.on('init-model', (event, data) => {
      vuex.lastSavedData = data
      const model = JSON.parse(data)
      Object.assign(vuex, model)

      // 重新渲染数据
      vuex.graph.data(vuex.graphData)
      vuex.graph.render()
    })
    vuex.resetTerm = this.resetTerm
  },

  beforeDestroy () {
    vuex.graph.destroy()
  },

  methods: {
    handleDrop (model, event) {
      if (model.isCollect) {
        const { nodes, edges, x, y, isCombo } = model.data
        const np = vuex.graph.getPointByClient(event.clientX, event.clientY)

        const DeltaX = np.x - x
        const DeltaY = np.y - y

        const IdMap = {}
        const before = vuex.graph.save()
        const nodeIds = []

        for (const node of nodes) {
          const nodeCfg = cloneDeep(node)
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
          const edgeConfig = cloneDeep(edge)

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
      } else {
        // 获取鼠标位置
        const { clientX, clientY } = event
        // 坐标转换成为视口坐标
        const { x, y } = vuex.graph.getPointByClient(clientX, clientY)

        // 增加节点
        addItem({
          x: x - model.width / 2,
          y: y - model.height / 2 - 15,
          ...cloneDeep(model)
        })
      }
    },
    // 画布大小发生变化时候触发
    handleResize () {
      const { clientHeight, clientWidth } = this.$refs.CanvasNode
      // 画布缩放
      vuex.graph.changeSize(clientWidth, clientHeight)
    },
    attachEventToGraph () {
      // 节点的右键菜单
      vuex.graph.on('node:contextmenu', (evt) => {
        evt.stopPropagation()
        this.clickEvt.type = 'node'
        this.selectedNode = evt.item
      })

      vuex.graph.on('combo:contextmenu', (evt) => {
        evt.stopPropagation()
        this.clickEvt.type = 'combo'
        this.clickEvt.combo = evt.item
      })

      vuex.graph.on('edge:contextmenu', (evt) => {
        evt.stopPropagation()
        this.clickEvt.type = 'edge'
        this.clickEvt.edge = evt.item
      })
      vuex.graph.on('canvas:contextmenu', (evt) => {
        evt.stopPropagation()
        this.clickEvt.type = 'canvas'
        this.clickEvt.clickEventX = evt.clientX
        this.clickEvt.clickEventY = evt.clientY
      })
      vuex.graph.on('node:click', (evt) => {
        this.selectedNode = evt.item
      })

      vuex.graph.on('node:dblclick', (evt) => {
        evt.stopPropagation()

        this.selectedNode = evt.item
        const model = this.selectedNode.getModel()
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
      })

      vuex.graph.on('keydown', (evt) => {
        evt.stopPropagation()

        if (vuex.running) {
          return
        }

        const delta = vuex.store.get('VUE_APP_DELTA_PX')
        let deltaX = 0
        let deltaY = 0

        switch (evt.key) {
          case 'Delete': {
            this.selectedNode = null
            const before = vuex.graph.save()
            const nodes = vuex.graph.findAllByState('node', 'selected')
            for (const node of nodes) {
              removeItem(node)
            }
            const after = vuex.graph.save()
            vuex.graph.pushStack('changedata', {
              before, after
            }, 'undo')
            break
          }
          case 'ArrowUp': {
            deltaX = 0
            deltaY = -delta
            break
          }
          case 'ArrowDown': {
            deltaX = 0
            deltaY = delta
            break
          }
          case 'ArrowLeft': {
            deltaX = -delta
            deltaY = 0
            break
          }
          case 'ArrowRight': {
            deltaX = delta
            deltaY = 0
            break
          }
          case 'c': {
            if (!evt.ctrlKey) break

            const nodes = vuex.graph.findAllByState('node', 'selected')
            const edges = vuex.graph.findAllByState('edge', 'selected')

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
            break
          }
          case 'v': {
            if (!evt.ctrlKey) break
            const before = vuex.graph.save()
            const { nodes, edges, isCombo } = vuex.copedItem
            const DeltaX = 100
            const DeltaY = 50

            const IdMap = {}
            const nodeIds = []

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

              // 增加节点
              addItem(edgeConfig, 'edge', false)
            }
            const after = vuex.graph.save()
            vuex.graph.pushStack('changedata', {
              before, after
            }, 'undo')

            vuex.copedItem = null
            if (isCombo) {
              addItem(nodeIds, 'combo')
            }
            break
          }
          case 'r': {
            if (!evt.ctrlKey) break
            this.fetchModels()
            break
          }
          case 'z': {
            if (!evt.ctrlKey) break
            vuex.toolbar.undo()
            vuex.graph.refresh()
            break
          }
          case 'y': {
            if (!evt.ctrlKey) break
            vuex.toolbar.redo()
            vuex.graph.refresh()
            break
          }
          default:
            break
        }

        if (deltaX !== 0 || deltaY !== 0) {
          const nodes = vuex.graph.findAllByState('node', 'selected')
          for (const node of nodes) {
            const model = node.getModel()
            node.updatePosition({
              x: model.x + deltaX,
              y: model.y + deltaY
            })
            const edges = node.getEdges()
            for (const edge of edges) {
              edge.refresh()
            }
          }
        }
      })
    },
    fetchModels () {
      const path = window.require('path')
      const modelFolder = path.normalize(path.resolve(vuex.store.get('VUE_APP_MODEL_PATH')))

      Object.keys(window.require.cache).forEach(key => {
        if (path.normalize(key).startsWith(modelFolder)) {
          delete window.require.cache[key]
        }
      })

      const models = window.require(modelFolder)

      // 找到对应的元素
      vuex.collect = models.find(p => p.label.startsWith('高级')).components.find(p => p.label === '收藏')

      this.$nextTick(() => {
        this.widgets = models
      })
    },
    initTerm () {
      const term = new Terminal({
        fontSize: 16,
        theme: xtermTheme.Violet_Light,
        fontFamily: 'consolas, inconsolata, menlo',
        letterSpacing: 0,
        padding: 5
      })
      vuex.term = term

      this.fitAddon = new FitAddon()
      term.loadAddon(this.fitAddon)
      term.open(document.getElementById('terminal'))
      this.fitAddon.fit()
    },
    copySelected () {
      if (vuex.term.hasSelection()) {
        this.$clipboard(vuex.term.getSelection())
        vuex.term.clearSelection()
      }
    },
    resetTerm () {
      this.termVisible = true
    },
    resizeTerm () {
      this.fitAddon.fit()
    },
    handleChange (val) {
      vuex.term.setOption('theme', xtermTheme[val])
    }
  }
}
</script>

<style lang="less" scoped>
.layout {
  position: relative;

  .header {
    width: 100vw;
    height: 48px;
    background: #EEE;
    border-bottom: 1px solid #CCC;
  }

  .body {
    width: 100vw;
    height: calc(100vh - 58px);
    display: flex;

    .siderbar {
      background: #E5E5E5;
    }

    .editor {
      background: transparent;
      flex: 1 1 0;
      padding: 0px;
    }
  }

  .footer {
    background: #DADADA;
    height: 10px;
    width: 100vw;
  }
}

.terminal-icon{
  position: fixed;
  font-size: 28px;
  cursor: pointer;
  right: 20px;
  bottom: 30px;
}
</style>
