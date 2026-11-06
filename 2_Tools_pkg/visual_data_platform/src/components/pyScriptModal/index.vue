<template>
  <JSPanel
    :visible="visible"
    :modal-id="modalId"
    :options="modalOptions"
    @resize="resizeCodeEditor"
    @close="close"
  >
    <a-row
      type="flex"
      style="height:100%;"
    >
      <a-col
        flex="300px"
        style="height:100%; padding:5px;"
      >
        <div class="groupbox">
          <span class="grouptitle">基本信息</span>
          <div>
            输入: args
          </div>
          <div>一个列表，对应输入锚点顺序</div>
          <div>注：锚点接收任意类型输入</div>
          <div style="margin-top:10px;">
            输出: outs
          </div>
          <div>一个列表，对应输出锚点的顺序, 多余锚点数的输出会被忽略</div>
          <div>注：锚点接收任意类型输出</div>
          <div style="margin-top:10px;">
            全局: store
          </div>
          <div>全局变量，循环时候可用于保存结果</div>
          <div style="margin-top:10px;">
            进度: reportProgress
          </div>
          <div>调用reportProgress(val),val为0-100的数值</div>
        </div>
        <div style="height:10px;" />
        <div class="groupbox">
          <span class="grouptitle">参数配置</span>
          <JsonVM
            :value="model.kwargs"
            :schema="model.kwargsSchemaProperties"
          />
        </div>
        <a-button
          size="small"
          block
          @click="runNode"
        >
          {{ node && node.hasState('runState')? '停止':'执行' }}
        </a-button>
        <a-checkbox
          :checked="model.disableModalAutoClose!==true"
          @change="setValue(model,'disableModalAutoClose',!$event.target.checked)"
        >
          运行时自动关闭窗体
        </a-checkbox>
      </a-col>
      <a-col
        flex="auto"
        style="height:100%;"
      >
        <div
          class="groupbox"
          style="height:calc(100% - 20px);width:calc(100% - 10px);"
        >
          <span class="grouptitle">脚本</span>
          <CodeEditor
            ref="codeEditor"
            v-model="model.kwargs.script"
            lang="python"
            theme="chrome"
            width="100%"
            height="100%"
            @keydown.native="onCoderKeyDown"
            @init="editorInit"
          />
        </div>
      </a-col>
    </a-row>
  </JSPanel>
</template>

<script>
import { runNode, stopGraph } from '@/utils/core'
import JSPanel from '@/components/jsPanel'
import JsonVM from '@/components/jsonVM'

export default {
  components: {
    JSPanel, JsonVM,
    CodeEditor: require('vue2-ace-editor')
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

  watch: {
    'model.kwargs.inputLength' (val) {
      const anchors = []
      for (let i = 0; i < val; i++) {
        anchors.push({ type: 'Any', label: '任意' })
      }
      this.node.update({
        inAnchor: anchors
      })
    },
    'model.kwargs.outputLength' (val) {
      const anchors = []
      for (let i = 0; i < val; i++) {
        anchors.push({ type: 'Any', label: '任意' })
      }
      this.node.update({
        outAnchor: anchors
      })
    }
  },

  methods: {
    editorInit () {
      require('brace/ext/language_tools') // language extension prerequsite...
      require('brace/mode/python') // language
      require('brace/mode/javascript') // language
      require('brace/theme/chrome')
      require('brace/snippets/python') // snippet
      require('brace/snippets/javascript') // snippet
    },
    close () {
      this.$destroy()
    },
    setValue (object, key, value) {
      this.$set(object, key, value)
    },
    resizeCodeEditor () {
      const editor = this.$refs.codeEditor.editor
      editor && editor.resize()
    },
    onCoderKeyDown (event) {
      if (event.ctrlKey === true && event.keyCode === 83) {
        event.preventDefault()
      }
    },
    runNode () {
      if (this.node.hasState('runState')) {
        stopGraph()
        return
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
.groupbox {
  padding:10px;
  margin: 10px 0;
	width: 100%;
	border:thin solid #ccc;
  font-size: 15px;
}

.grouptitle {
  display: table;
  padding: 0 6px;
	margin-top:-21px;
  margin-left:10px;
  margin-right: auto;
	text-align: left;
  background-color:#FFFFFF;
}
</style>

