<template>
  <JSPanel
    :visible="visible"
    :modal-id="modalId"
    :options="modalOptions"
    @close="close"
  >
    <div style="padding:10px;">
      <div
        :id="tid"
        style="width:100%; height:60vh;"
      />
    </div>

    <template slot="footerToolbar">
      <div style="width:100%;">
        <input
          style="font-size:16px;"
          type="file"
          accept=".xlsx"
          @change="uploadExcel"
        >

        <a-button
          type="primary"
          style="float:right;width:200px;margin-left:10px;"
          @click="runNode"
        >
          运行
        </a-button>

        <a-button
          type="danger"
          style="float:right;width:200px;"
          @click="saveConfig"
        >
          保存配置
        </a-button>
      </div>
    </template>
  </JSPanel>
</template>

<script>
import { runNode, stopGraph } from '@/utils/core'
import JSPanel from '@/components/jsPanel'
import LuckyExcel from 'luckyexcel'

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
      tid: 'tid' + this.uuidv4(),

      options: {
        title: '数据表格', // 设定表格名称
        lang: 'zh', // 设定表格语言
        showtoolbar: false,
        showtoolbarConfig: {
          undoRedo: true,
          font: true,
          paintFormat: true,
          fontSize: true, // '字号大小'
          bold: true, // '粗体 (Ctrl+B)'
          italic: true, // '斜体 (Ctrl+I)'
          border: true, // '边框'
          horizontalAlignMode: true, // '水平对齐方式'
          verticalAlignMode: true, // '垂直对齐方式'
          function: true, // '公式'
          frozenMode: true, // '冻结方式'
          sortAndFilter: true, // '排序和筛选'
          splitColumn: true, // '分列'
          findAndReplace: true // '查找替换'
        },
        showsheetbar: false,
        showinfobar: false
      }
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

  mounted () {
    const id = this.tid
    const options = this.model.kwargs.table || this.options
    options.container = id
    setTimeout(() => {
      window.luckysheet.create(options)
    }, 2000)
  },

  methods: {
    close () {
      this.$destroy()
    },

    saveConfig () {
      this.model.kwargs.table = window.luckysheet.toJson()
      // 提取数据
      let colQty = 0
      for (let i = 0; i < 60; i++) {
        if (window.luckysheet.getCellValue(0, i) !== null) {
          colQty += 1
        }
      }

      let rowQty = 0
      const maxRowCount = window.luckysheet.getSheetData().length
      for (let i = 0; i < maxRowCount; i++) {
        if (window.luckysheet.getCellValue(i, 0) !== null) {
          rowQty += 1
        }
      }
      const data = []
      for (let i = 0; i < rowQty; i++) {
        const rowData = []
        for (let j = 0; j < colQty; j++) {
          rowData.push(window.luckysheet.getCellValue(i, j))
        }
        data.push(rowData)
      }
      this.model.kwargs.data = data
    },

    uploadExcel (evt) {
      const files = evt.target.files
      if (files == null || files.length === 0) {
        alert('没有选择文件')
        return
      }

      const name = files[0].name
      const suffixArr = name.split('.'); const suffix = suffixArr[suffixArr.length - 1]
      if (suffix !== 'xlsx') {
        alert('目前只支持XLSX文件格式')
        return
      }

      const that = this
      LuckyExcel.transformExcelToLucky(files[0], function (exportJson, luckysheetfile) {
        if (exportJson.sheets == null || exportJson.sheets.length === 0) {
          alert('读取失败!')
          return
        }
        window.luckysheet.destroy()

        const options = that.model.kwargs.table || that.options
        options.data = exportJson.sheets
        window.luckysheet.create(options)
      })
    },

    runNode () {
      if (this.node.hasState('runState')) {
        stopGraph()
        return
      }
      this.saveConfig()

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

