import Vue from 'vue'
import App from './App.vue'
import router from './router'
import vuex from './vuex'
// import { addProtect } from '@/utils/secure/secure'

import xDataGrid from 'x-data-grid'
import 'x-data-grid/lib/xDataGrid.css'
Vue.use(xDataGrid)

import VueExcelEditor from 'vue-excel-editor'
Vue.use(VueExcelEditor)

import './plugins/antd'
import './plugins/dragdrop'
import './plugins/resize'
import './plugins/modal'
import './plugins/contextmenu'
import './plugins/jsPanel'
import './plugins/clipboard'
import './plugins/drawBoard'

// 引入定制节点
import './utils/graphic/node'
// 引入定制交互
import './utils/graphic/behavior'
import './utils/graphic/edge'

import '@/assets/icons/iconfont.css'
import 'xterm/css/xterm.css'
import 'jspanel4/es6module/jspanel.min.css'
import 'cropperjs/dist/cropper.css'

const Store = window.require('electron-store')
const schema = {
  'VUE_APP_PYTHON': {
    type: 'string',
    default: 'pyr'
  },
  'VUE_APP_MODEL_PATH': {
    type: 'string',
    default: './core/model'
  },
  'VUE_APP_DELTA_PX': {
    type: 'number',
    default: 1
  },
  'MACHINE_ID': {
    type: 'string',
    default: ''
  },
  'ACTIVE_KEY': {
    type: 'string',
    default: ''
  }
}

vuex.copedItem = null
vuex.store = new Store({ schema })

// 设置环境变量
// console.log(process.env.PATH)

// addProtect(router)

Vue.config.productionTip = false
// id生成器
Vue.prototype.uuidv4 = function () {
  return Date.now().toString() + (crypto.getRandomValues(new Uint8Array(1))[0] & 15).toString(6)
}

new Vue({
  router,
  render: h => h(App)
}).$mount('#app')
