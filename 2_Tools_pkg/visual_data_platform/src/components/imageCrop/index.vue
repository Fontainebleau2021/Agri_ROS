<template>
  <JSPanel
    :visible="visible"
    :modal-id="modalId"
    :options="modalOptions"
    @close="close"
  >
    <div style="padding:10px;">
      <a-table
        style="background:white; borderRadius: 5px;margin-bottom:10px;"
        :columns="columns"
        :data-source="[clip]"
        :pagination="{hideOnSinglePage:true}"
        bordered
      />

      <div style="width:100%;">
        <canvas
          ref="image"
          style="width:100%;"
        >
          <a-button @click="runNode">
            裁剪
          </a-button>
        </canvas>
      </div>

      <a-button
        block
        type="primary"
        style="margin-top:10px;"
        @click="runNode"
      >
        运行
      </a-button>
    </div>
  </JSPanel>
</template>

<script>
import { runNode, stopGraph } from '@/utils/core'
import JSPanel from '@/components/jsPanel'
import Cropper from 'cropperjs'

import parseJson from '@/utils/json'
const { PythonShell } = window.require('python-shell')
const { set } = require('lodash')

export default {
  components: {
    JSPanel
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
          headerTitle: '图像裁剪'
        }
      }
    }
  },

  data () {
    return {
      visible: true,
      modalId: 'modal' + this.uuidv4(),
      clip: {
        x: 0,
        y: 0,
        width: 1920,
        height: 1080
      },
      columns: [
        { title: '左边(px)', dataIndex: 'x' },
        { title: '上边(px)', dataIndex: 'y' },
        { title: '宽度(px)', dataIndex: 'width' },
        { title: '高度(px)', dataIndex: 'height' }
      ]
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
    'model.kwargs.src' (path) {
      const cv = this.$refs.image
      const ctx = cv.getContext('2d')
      var img = new Image()
      const that = this
      img.onload = function () {
        var imgWidth = img.width
        var imgHeight = img.height
        ctx.clearRect(0, 0, cv.width, cv.height)
        cv.width = imgWidth
        cv.height = imgHeight
        ctx.drawImage(img, 0, 0)
        that.enableCrop()
      }
      img.src = path
    }
  },

  created () {
    this.model.kwargs.src = ''
    this.initialize()
  },

  methods: {
    close () {
      this.$destroy()
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
                set(nodeModel.kwargs, setter.keys[i], setter.values[setter.values.length - 1])
              } else {
                set(nodeModel.kwargs, setter.keys[i], setter.values[i])
              }
            }
          }
        }
      })
    },

    enableCrop () {
      const that = this
      const cv = this.$refs.image
      that.clip = that.model.kwargs.clip
      new Cropper(cv, {
        data: that.clip,
        autoCrop: true,
        crop (event) {
          that.clip = event.detail
          that.model.kwargs.x = Math.round(that.clip.x)
          that.model.kwargs.y = Math.round(that.clip.y)
          that.model.kwargs.width = Math.round(that.clip.width)
          that.model.kwargs.height = Math.round(that.clip.height)
          that.model.kwargs.clip = {
            ...that.clip,
            x: that.model.kwargs.x,
            y: that.model.kwargs.y,
            width: that.model.kwargs.width,
            height: that.model.kwargs.height
          }
          that.clip = that.model.kwargs
        }
      })
    },

    setValue (object, key, value) {
      this.$set(object, key, value)
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

