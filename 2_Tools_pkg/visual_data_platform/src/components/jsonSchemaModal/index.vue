<template>
  <JSPanel
    :visible="visible"
    :modal-id="modalId"
    :options="modalOptions"
    @close="close"
  >
    <div style="height:100%;width:100%;">
      <div
        style="height:calc(100% - 35px); padding:10px 10px 0 10px;overflow:auto;"
      >
        <a-row
          v-if="model.dynamicInput"
          type="flex"
          align="middle"
          style="margin: 5px 0;"
        >
          <a-col flex="80px">
            输入锚点:
          </a-col>
          <a-col
            :style="{
              flex: '1 1 0',
              textAlign: 'right'
            }"
          >
            <a-input-number
              :default-value="model.inAnchor.length"
              v-bind="model.kwargsSchemaProperties.input.bind || {}"
              @change="onChangeInAnchor"
            />
          </a-col>
        </a-row>
        <JsonVM
          :value="model.kwargs"
          :schema="model.kwargsSchemaProperties"
          :callbacks="callbacks"
        />
      </div>
      <a-row style="height: 35px; padding:5px 10px;border-top: 1px solid #F0F0F0">
        <a-col :span="14">
          <a-checkbox
            :checked="model.disableModalAutoClose!==true"
            @change="setValue(model,'disableModalAutoClose',!$event.target.checked)"
          >
            同时关闭窗体
          </a-checkbox>
        </a-col>
        <a-col :span="10">
          <a-button
            size="small"
            :disabled="model.virtual"
            block
            @click="runNode"
          >
            {{ node && node.hasState('runState')? '停止':'执行' }}
          </a-button>
        </a-col>
      </a-row>
    </div>
  </JSPanel>
</template>

<script>
import { runNode, stopGraph } from '@/utils/core'
import JSPanel from '@/components/jsPanel'
import JsonVM from '@/components/jsonVM'
import parseJson from '@/utils/json'

const { readFilePathDialog, saveFilePathDialog } = require('@/utils/file')

const { PythonShell } = window.require('python-shell')
const { set } = require('lodash')

export default {
  components: {
    JsonVM, JSPanel
  },
  props: {
    node: {
      type: Object,
      default: () => { return undefined }
    },
    modalOptions: {
      type: Object, default: () => {
        return {
          contentSize: '600 400',
          headerTitle: '配置面板'
        }
      }
    }
  },

  data () {
    return {
      visible: true,
      modalId: 'modal' + this.uuidv4()
    }
  },

  computed: {
    model () {
      if (this.node) {
        return this.node.getModel()
      }
      return {}
    }
  },

  created () {
    this.initialize()
    this.callbacks = {
      openFile: this.openFile,
      saveFile: this.saveFile
    }
  },

  methods: {
    openFile (object, key, options = {}) {
      readFilePathDialog(options).then(filePath => {
        if (!filePath) return
        this.$set(object, key, filePath)
      })
    },
    saveFile (object, key, options = {}) {
      saveFilePathDialog(options).then(filePath => {
        if (!filePath) return
        this.$set(object, key, filePath)
      })
    },
    initialize () {
      const nodeModel = this.model

      if (!nodeModel.KwargsInitialize || nodeModel.KwargsInitialize.length === 0) {
        return
      }

      const node = this.node
      const vuex = require('@/vuex')
      // 获取入边
      const edges = node.getInEdges()
      const inStore = nodeModel.inAnchor.map(() => { return null })

      // 按照位置插入数据
      for (const edge of edges) {
        const edgeModel = edge.getModel()
        const inNode = edge.getSource().getModel()
        inStore[edgeModel.targetAnchor] = {
          file: `${vuex.appId}_${edgeModel.source}`,
          key: edgeModel.sourceAnchor - inNode.inAnchor.length
        }
      }
      const pyShell = new PythonShell('KwargsInitialization.py', {
        pythonPath: vuex.store.get('VUE_APP_PYTHON'),
        scriptPath: vuex.store.get('VUE_APP_MODEL_PATH')
      })
      pyShell.send(JSON.stringify({
        inStore,
        setters: nodeModel.KwargsInitialize
      }))

      pyShell.on('message', function (message) {
        if (message.startsWith('$node-Initialization$')) {
          const msg = message.substring(22)
          const setters = parseJson(msg)

          for (const setter of setters) {
            for (const i in setter.keys) {
              if (i >= setter.values.length) {
                set(nodeModel.kwargsSchemaProperties, setter.keys[i], setter.values[setter.values.length - 1])
              } else {
                set(nodeModel.kwargsSchemaProperties, setter.keys[i], setter.values[i])
              }
            }
          }
        }
      })
    },
    close () {
      this.$destroy()
    },
    setValue (object, key, value) {
      this.$set(object, key, value)
    },
    onChangeInAnchor (e) {
      if (this.model.inAnchor.length > e) {
        this.model.inAnchor.splice(e, 1)
        this.model.height -= 20
      } else {
        this.model.inAnchor.push(this.model.inAnchorTemplate)
        this.model.height += 20
      }
      this.node.refresh()
    },
    runNode () {
      if (this.node.hasState('runState')) {
        stopGraph()
        return
      }

      // 检查必选参数是否填写
      for (const key in this.model.kwargsSchemaProperties) {
        const checker = this.model.kwargsSchemaProperties[key]
        if (checker.required !== false) {
          if (this.model.kwargs[key] === undefined) {
            this.$message.error(`必选参数：${checker.title}没有填写！`)
            return
          }
          if (this.model.kwargs[key].toString() === '') {
            this.$message.error(`必选参数：${checker.title}没有填写！`)
            return
          }
        }
      }

      const vuex = require('@/vuex')
      runNode(
        this.node,
        vuex.appId,
        vuex.env === 'development' ? 'train' : 'prod')

      if (!this.model.disableModalAutoClose) {
        this.visible = false
      }
    }
  }
}
</script>

<style lang="less" scoped>
.icon-btn {
  cursor: pointer;
  font-size: 15px;
  margin: 0 5px;
}
.window-footer {
  height: 6px;
  user-select:none;
  padding: 0 10px;
  border-top: 1px solid #E0E0E0;
  background: #F0F0F0;
}
</style>

