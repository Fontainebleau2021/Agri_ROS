import Vue from 'vue'
import Term from './Term.vue'

import './plugins/clipboard'

import './plugins/resize'
import 'xterm/css/xterm.css'

new Vue({
  render: h => h(Term)
}).$mount('#app')
