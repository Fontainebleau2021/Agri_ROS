<template>
  <div class="header">
    <!-- 标题 -->
    <div class="header-text">
      <!-- {{ vuex.env==='production'? '应用模型：':'开发模型：' }} -->
      {{ header }}
      <span style="color:#ED3A4A">{{ vuex.running? '(画布锁定)':'' }}</span>
    </div>
    <!-- 操作 -->
    <div
      class="actions"
      style="z-index:10"
    >
      <icon-font
        type="icon-timer"
        class="icon-btn"
        style="font-size: 21px;"
        @click="timerVisible=true"
      />
      <icon-font
        type="icon-save"
        class="icon-btn"
        style="font-size: 21px;"
        @click="saveModel"
      />
      <icon-font
        v-if="!vuex.running"
        type="icon-play"
        class="icon-btn"
        style="color:#00aa00;"
        @click="runConfirmVisible=true"
      />
      <icon-font
        v-else
        type="icon-stop"
        class="icon-btn"
        style="color:red"
        @click="stopGraph"
      />
      <icon-font
        type="icon-info"
        class="icon-btn"
        style="color:#234599"
        @click="infoVisible=true"
      />
    </div>

    <JSPanel
      :visible.sync="timerVisible"
      modal-id="timerSetterModal"
      :options="{
        contentSize:'300 auto',
        resizeit:false,
        headerControls:{
          maximize:'remove',
          normalize:'remove',
          minimize:'remove',
        },
        headerTitle:'计时器设置'
      }"
      @close="timerVisible = false"
    >
      <div style="padding:10px; text-align:left">
        <a-input
          v-model="vuex.timer.interval"
          addon-after="秒"
        >
          <IconFont
            slot="addonBefore"
            type="icon-timer"
          />
        </a-input>
        <a-checkbox
          style="margin-top:-10px;"
          :checked="vuex.timer.enabled"
          @change="onChangeTimer"
        >
          启用定时器
        </a-checkbox>
      </div>
    </JSPanel>

    <JSPanel
      :visible.sync="infoVisible"
      modal-id="appInfoModal"
      :options="{
        contentSize:'300 auto',
        resizeit:false,
        headerControls:{
          maximize:'remove',
          normalize:'remove',
          minimize:'remove',
        },
        headerTitle:'应用基本信息'
      }"
      @close="infoVisible = false"
    >
      <div
        style="background:#F0F0F0; padding:10px;"
      >
        <a-input
          v-model="vuex.info.name"
          placeholder="应用名称"
        />
        <a-textarea
          v-model="vuex.info.description"
          placeholder="应用简介"
          allow-clear
          :rows="4"
          style="margin-top:5px"
        />
      </div>
    </JSPanel>

    <JSPanel
      :visible.sync="runConfirmVisible"
      modal-id="appRunConfirmModal"
      :options="{
        contentSize:'300 auto',
        resizeit:false,
        headerControls:{
          maximize:'remove',
          normalize:'remove',
          minimize:'remove',
          smallify:'remove'
        },
        headerTitle:'确定启动计算'
      }"
      @close="runConfirmVisible = false"
    >
      <div
        style="height:50px;background:#F0F0F0; padding:10px;"
      >
        <a-button
          style="float:right; margin:5px;"
          type="primary"
          size="small"
          @click="runGraph"
        >
          启动
        </a-button>
        <a-button
          style="float:right; margin:5px;"
          size="small"
          @click="runConfirmVisible = false"
        >
          取消
        </a-button>
      </div>
    </JSPanel>
  </div>
</template>

<script>
import IconFont from '@/components/iconfont'
import vuex from '@/vuex'
import { runGraph, stopGraph, generateCode, getRunConfig } from '@/utils/core'
const {
  saveFile,
  saveFileDialog,
  readFilePathDialog
} = require('@/utils/file')

const { remote, ipcRenderer } = window.require('electron')
const { dialog } = remote
import JSPanel from '@/components/jsPanel'

export default {
  components: {
    IconFont, JSPanel
  },

  props: {
    header: {
      type: String,
      default: ''
    }
  },

  data () {
    return {
      vuex: vuex,
      infoVisible: false,
      timerVisible: false,
      runConfirmVisible: false
    }
  },

  created () {
    ipcRenderer.on('save-model', () => {
      this.saveModel()
    })

    ipcRenderer.on('save-run-config', () => {
      this.saveModelConfig()
    })

    vuex.forceClose = false
    window.addEventListener('beforeunload', evt => {
      if (vuex.forceClose) return

      const newData = vuex.graph.save()
      const data = JSON.stringify({
        env: vuex.env,
        appId: vuex.appId,
        groupId: vuex.groupId,
        info: vuex.info,
        nodeId: vuex.nodeId,
        graphData: newData
      }, undefined, 2)

      // 如果没有发生变化
      if (data === vuex.lastSavedData) return

      evt.returnValue = false
      setTimeout(() => {
        dialog.showMessageBox({
          message: '您还有工作没有保存，是否退出?',
          buttons: ['退出', '取消']
        }).then(({ response }) => {
          if (response === 0) {
            vuex.forceClose = true
            remote.getCurrentWindow().close()
          }
        })
      })
    })
  },

  methods: {
    saveModel () {
      const newData = vuex.graph.save()
      const data = JSON.stringify({
        env: vuex.env,
        appId: vuex.appId,
        groupId: vuex.groupId,
        info: vuex.info,
        nodeId: vuex.nodeId,
        graphData: newData
      }, undefined, 2)

      // 如果没有发生变化
      if (data === vuex.lastSavedData) return

      if (vuex.saveModelPath) {
        saveFile(vuex.saveModelPath, data).then(() => {
          vuex.lastSavedData = data
        })
      } else {
        const extension = vuex.env === 'development' ? 'lab' : 'lab'
        saveFileDialog({
          title: '保存模型',
          filters: [
            { name: 'model', extensions: [extension] },
            { name: 'All File', extensions: ['*'] }
          ]
        }, data).then((saveModelPath) => {
          vuex.saveModelPath = saveModelPath
          vuex.lastSavedData = data
        })
      }
    },

    saveModelConfig () {
      const data = getRunConfig(
        vuex.graph.getNodes(),
        vuex.appId,
        vuex.timer,
        vuex.env === 'development' ? 'train' : 'prod')

      readFilePathDialog({
        title: '选择代码保存文件夹',
        properties: ['openDirectory']
      }).then(folder => {
        generateCode(folder, data)
      })
    },

    runGraph () {
      runGraph(
        vuex.graph.getNodes(),
        vuex.appId,
        vuex.timer,
        vuex.env === 'development' ? 'train' : 'prod')
      this.runConfirmVisible = false
    },
    stopGraph () {
      stopGraph()
    },
    onChangeTimer () {
      this.$set(this.vuex.timer, 'enabled', !this.vuex.timer.enabled)
    }
  }
}
</script>

<style lang="less" scoped>
.header {
  text-align: center;
  line-height: 48px;

  .actions {
    float: right;
    margin-right: 20px;
  }

  .header-text {
    display: inline-block;
    z-index: 0;
  }

  .icon-btn {
    font-size: 20px;
    padding: 5px;
    margin: 5px;
    cursor: pointer;
  }
}
</style>
