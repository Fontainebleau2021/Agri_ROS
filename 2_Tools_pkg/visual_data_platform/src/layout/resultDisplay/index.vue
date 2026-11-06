<template>
  <JSPanel
    :visible="true"
    :modal-id="modalId"
    :options="{
      contentSize:'600 400',
      headerTitle: nodeLabel + '-结果可视化',
      dragit: {
        containment: [0, -2000,-2000,-2000]
      },
      resizeit: {
        handles: 'e, s, w, se, sw'
      }
    }"
    @close="close"
  >
    <a-tabs
      v-model="selectIndex"
      tab-position="left"
      :style="{ width: '100%', height:'100%'}"
      @change="changeSelect"
    >
      <a-tab-pane
        v-for="(item,index) in visualizationOptions"
        :key="index"
      >
        <span slot="tab">
          {{ item.label }}
        </span>
        <keep-alive>
          <div class="visualization">
            <template v-if="(fetchFinished[index]|| continueRender) && cached[index]">
              <template v-if="item.type==='chart'">
                <Chart
                  ref="chart"
                  v-bind="cached[index]"
                />
              </template>
              <template v-if="item.type==='g2plt'">
                <G2plt
                  v-bind="cached[index]"
                />
              </template>
              <template v-if="item.type==='heatmap'">
                <Chart
                  ref="chart"
                  v-bind="cached[index]"
                />
              </template>
              <template v-if="item.type==='table'">
                <div style="width:100%;height:100%;overflow:auto;">
                  <a-table
                    v-bind="cached[index]"
                    :row-class-name="(record, index) => {
                      return index % 2 === 0 ? 'grey' : ''
                    }"
                  />
                </div>
              </template>
              <template v-if="item.type==='data-grid'">
                <div style="width:100%;height:100%;overflow:auto;">
                  <x-data-grid
                    height="calc(100% - 42px)"
                    v-bind="cached[index]"
                    :page-size="50"
                  />
                </div>
              </template>
              <template v-if="item.type==='image'">
                <img
                  v-bind="cached[index]"
                  style="maxWidth:100%;"
                >
              </template>
              <template v-if="item.type==='text'">
                <div
                  style="height:100%;overflow:auto;"
                  v-bind="cached[index]"
                  v-html="cached[index].text"
                />
              </template>
            </template>
          </div>
        </keep-alive>
      </a-tab-pane>
    </a-tabs>
  </JSPanel>
</template>

<script>
import Chart from '@/components/chart'
import G2plt from '@/components/g2plt'

const { PythonShell } = window.require('python-shell')
const _ = require('lodash')
const { addStateChangeHandler, removeStateChangeHandler } = require('@/vuex')

import JSPanel from '@/components/jsPanel'
import parseJson from '@/utils/json'

export default {
  components: {
    Chart, JSPanel, G2plt
  },

  props: {
    visualizationOptions: {
      type: Array, required: true
    },
    nodeId: {
      type: String, default: ''
    },
    nodeLabel: {
      type: String, default: ''
    }
  },

  data () {
    return {
      // 当前选择的标签
      selectIndex: 0,
      // 循环执行的时候之前的结果在刷新数据的时候保留
      continueRender: false,
      fetchFinished: {},
      // 一次性只能执行一个PyShell
      pyShell: null,
      // 缓存数据
      cached: {},
      modalId: 'modal' + this.uuidv4()
    }
  },

  created () {
    this.cached = {}
    // 上来先获取一次数据
    this.fetchData(this.selectIndex)
    this.continueRender = false
    addStateChangeHandler(this.stateChangeHandler)
  },

  beforeDestroy () {
    removeStateChangeHandler(this.stateChangeHandler)
  },

  methods: {
    stateChangeHandler (node, state, value) {
      if (node === this.nodeId && state === 'runState' && value) {
        this.continueRender = true
        this.fetchData(this.selectIndex)
      }
    },
    changeSelect (key) {
      if (!this.cached[key]) {
        this.fetchData(key)
        return
      }

      for (const chart of this.$refs.chart) {
        chart.reInit()
      }
    },
    fetchData (key) {
      // 拿到绘图配置
      const visualizationOption = this.visualizationOptions[key]
      // 正在获取数据
      this.fetchFinished[key] = false
      // 获取appId
      const vuex = require('@/vuex')
      const filePath = `${vuex.appId}_${this.nodeId}`

      // 启动脚本
      const pyShell = new PythonShell('runVisualization.py', {
        pythonPath: vuex.store.get('VUE_APP_PYTHON'),
        scriptPath: vuex.store.get('VUE_APP_MODEL_PATH')
      })
      // 发送数据
      pyShell.send(JSON.stringify({
        filePath,
        usedKeys: visualizationOption.usedKeys,
        transformer: visualizationOption.transformer
      }))

      const that = this

      // 收消息
      pyShell.on('message', function (message) {
        if (message.startsWith('$node-visualization$')) {
          const msg = message.substring(21)
          const value = parseJson(msg)

          if (visualizationOption.propKey.length !== value.length) {
            console.log('notFinished')
            that.fetchFinished[key] = true
            return
          }

          const propsClone = { ...visualizationOption.props }

          try {
            for (const i in visualizationOption.propKey) {
              if (visualizationOption.propKey[i][0] === '..') {
                _.set(visualizationOption, visualizationOption.propKey[i].slice(1), value[i])
              } else {
                _.set(propsClone, visualizationOption.propKey[i], value[i])
              }
            }
          } catch (e) {
            vuex.log(e)
          }

          if (visualizationOption.type === 'table') {
            propsClone.columns.forEach(column => {
              column.sorter = function (a, b) {
                if (a[column.dataIndex] < b[column.dataIndex]) return -1
                if (a[column.dataIndex] > b[column.dataIndex]) return 1
                return 0
              }
            })
          }
          that.$set(that.cached, key, propsClone)
          that.fetchFinished[key] = true
          that.$forceUpdate()
        }
      })

      pyShell.on('error', (e) => { vuex.error(e) })

      // 关闭其他的
      if (this.pyShell) {
        this.pyShell.kill()
      }
      this.PythonShell = pyShell
    },
    close () {
      this.$destroy()
    }
  }
}
</script>

<style lang="less" scoped>
.visualization {
  padding: 5px 0;
  height: 100%;
  width: calc(100% - 10px);
  overflow: auto;
}

</style>

<style lang="less">
.ant-tabs-left-content {
  height: 100% !important;

  .ant-tabs-tabpane-active{
    height: 100% !important;
  }

  .ant-tabs-tabpane-inactive{
    height: 0 !important;
  }
}

.ant-table-small{
  border: none !important;
}
</style>

