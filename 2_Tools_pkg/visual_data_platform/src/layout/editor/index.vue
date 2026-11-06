<template>
  <div style="height:100%;width:100%;">
    <div
      id="editor-container"
      @contextmenu.prevent="nodeContextMenu"
    />

    <JSPanel
      :visible.sync="documentVisible"
      modal-id="documentModal"
      :options="{
        headerTitle: '文档',
        contentSize:'700 500',
        dragit: {
          containment: [0, 0, 0, 0]
        }
      }"
      @close="documentVisible = false;"
    >
      <iframe
        style="width:100%;height:100%;margin:-5px -1px;"
        name="Document"
        :src="helpDocument"
      />
    </JSPanel>

    <JSPanel
      :visible.sync="codeVisible"
      modal-id="codeModal"
      :keep-dom="true"
      :options="{
        headerTitle: '编辑器',
        contentSize:'700 500',
        dragit: {
          containment: [0, 0,0,0]
        }
      }"
      @resize="resizeCodeEditor"
      @close="codeVisible = false"
    >
      <CodeEditor
        ref="codeEditor"
        v-model="codeContent"
        :lang="codeLang"
        theme="chrome"
        width="100%"
        height="100%"
        @keydown.native="onCoderKeyDown"
        @init="editorInit"
      />
    </JSPanel>

    <JSPanel
      :visible.sync="widgetNameModalVisible"
      modal-id="widgetNameModal"
      :options="{
        header:false,
        contentSize:'300 auto',
        resizeit:false,
        dragit: {
          containment: [0, -2000,-2000,-2000]
        }
      }"
      @close="widgetNameModalVisible = false"
    >
      <div>
        <a-row
          class="jsPanel-titlebar"
          style="height:30px;userSelect:none;padding: 0 5px;"
          align="middle"
          type="flex"
        >
          请输入新的模块名称
        </a-row>
        <div
          style="height:80px;background:#F0F0F0; padding:10px;"
        >
          <a-input
            v-model="widgetName"
            placeholder="模块名称"
          />
          <a-button
            style="float:right; margin:5px;"
            type="primary"
            size="small"
            @click="changeWidgetName"
          >
            确定
          </a-button>
          <a-button
            style="float:right; margin:5px;"
            size="small"
            @click="widgetNameModalVisible = false"
          >
            取消
          </a-button>
        </div>
      </div>
    </JSPanel>

    <JSPanel
      :visible.sync="comboNameModalVisible"
      modal-id="comboNameModal"
      :options="{
        header:false,
        contentSize:'300 auto',
        resizeit:false,
        dragit: {
          containment: [0, -2000,-2000,-2000]
        }
      }"
      @close="comboNameModalVisible = false"
    >
      <div>
        <a-row
          class="jsPanel-titlebar"
          style="height:30px;userSelect:none;padding: 0 5px;"
          align="middle"
          type="flex"
        >
          请输入新的组合名称
        </a-row>
        <div
          style="height:80px;background:#F0F0F0; padding:10px;"
        >
          <a-input
            v-model="comboName"
            placeholder="组合名称"
          />
          <a-button
            style="float:right; margin:5px;"
            type="primary"
            size="small"
            @click="changeComboName"
          >
            确定
          </a-button>
          <a-button
            style="float:right; margin:5px;"
            size="small"
            @click="comboNameModalVisible = false"
          >
            取消
          </a-button>
        </div>
      </div>
    </JSPanel>
  </div>
</template>

<script>
const menuUtil = require('./contextmenu').default
const { saveFile } = require('@/utils/file')
import vuex from '@/vuex'

import JSPanel from '@/components/jsPanel'

export default {
  components: {
    JSPanel,
    CodeEditor: require('vue2-ace-editor')
  },

  props: {
    clickEvent: {
      type: Object, required: true
    },
    selectedNode: {
      type: Object, default: () => { return {} }
    }
  },

  data () {
    return {
      helpDocument: '',
      codeLang: 'python',
      codeContent: '',
      codeContentOld: '',
      configVisible: false,
      widgetName: '',
      widgetNameModalVisible: false,
      comboName: '',
      comboNameModalVisible: false,
      codeVisible: false,
      documentVisible: false,
      running: false,
      editModal: null
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

    nodeContextMenu (event) {
      this.running = vuex.running

      switch (this.clickEvent.type) {
        case 'node':
          this.$contextmenu(menuUtil.constructNodeMenu.call(this, event))
          break

        case 'edge':
          this.$contextmenu(menuUtil.constructEdgeMenu.call(this, event))
          break

        case 'combo':
          this.$contextmenu(menuUtil.constructComboMenu.call(this, event))
          break

        default:
          this.$contextmenu(menuUtil.constructCanvasMenu.call(this, event))
          break
      }

      return false
    },

    onCoderKeyDown (event) {
      if (!this.codeVisible) return
      if (!this.editModal) return

      if (event.ctrlKey === true && event.keyCode === 83) {
        event.preventDefault()

        if (this.codeContent.trim().length === 0) return
        const model = this.editModal
        saveFile(this.codeLang === 'python' ? model.pythonPath : model.configPath, this.codeContent)
      }
    },

    changeWidgetName () {
      this.selectedNode.update({
        label: this.widgetName
      })
      this.widgetNameModalVisible = false
    },

    changeComboName () {
      this.clickEvent.combo.update({
        label: this.comboName
      })
      this.comboNameModalVisible = false
    },

    resizeCodeEditor () {
      const editor = this.$refs.codeEditor.editor
      editor && editor.resize()
    }
  }

}
</script>

<style lang="less" scoped>
#editor-container {
  width: 100%;
  height: 100%;
}
</style>

<style lang="less">
.vm--overlay[data-modal="term"] {
  display: none;
  background: red;
}
</style>
