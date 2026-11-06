const chalk = window.require('chalk')
const forcedChalk = new chalk.Instance({
  enabled: true,
  level: 2
})

const store = {
  env: 'development',
  term: null,
  termCount: 0,
  termState: 'grey',
  appId: '',
  nodeId: 1,
  groupId: 1,
  running: false,
  info: {},
  timer: {
    interval: 10,
    enabled: false
  }
}

// console函数
store.log = function (msg) {
  if (!store.term) return
  store.term.writeln(msg)
  store.termCount++
  store.termState = 'grey'
}

store.error = function (msg) {
  if (!store.term) return
  store.term.writeln(forcedChalk.red(msg))
  store.termCount++
  store.termState = 'red'
}

store.success = function (msg) {
  if (!store.term) return
  store.term.writeln(forcedChalk.blue(msg))
  store.termCount++
  store.termState = 'grey'
}

store.finish = function (msg) {
  if (!store.term) return
  store.term.writeln(forcedChalk.green(msg))
  store.termCount++
  store.termState = 'green'
}

store.clearTerm = function () {
  if (!store.term) return
  store.term.clear()
  store.termCount = 0
  store.termState = 'grey'
}

// 状态变化触发
const stateChangeHandlers = []

store.broadcastStateChange = function (node, state, value) {
  for (const hander of stateChangeHandlers) {
    hander(node, state, value)
  }
}

store.addStateChangeHandler = function (hander) {
  stateChangeHandlers.push(hander)
}

store.removeStateChangeHandler = function (hander) {
  const index = stateChangeHandlers.indexOf(hander)
  if (index !== -1) {
    stateChangeHandlers.splice(index, 1)
  }
}

module.exports = store
