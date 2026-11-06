<template>
  <div id="app">
    <a-config-provider :locale="zhCN">
      <router-view />
    </a-config-provider>
    <v-dialog />
  </div>
</template>

<script>
import zhCN from 'ant-design-vue/lib/locale-provider/zh_CN'

export default {
  data () {
    return {
      zhCN
    }
  }
}
</script>

<style lang="less">
html{
  user-select:none;
}

#luckysheet-input-box {
  z-index: 999999 !important;
}

*{
  font-family: consolas, inconsolata, menlo, FontAwesome !important;
}

.grey {
  background: #eee;
}

.menu_item .menu_item_expand_icon {
  font-size: inherit !important;
}

body {
  margin: 0;
  padding: 0;
  overflow: hidden;
}

.fill-parent {
  width: 100%;
  height: 100%;
}

#toolbarContainer {
  position: absolute;
  left: 50vw;
  bottom: 70px;
  width: 600px;
}

.modal-action-button,.vue-dialog-button{
  outline: none;

  &:focus{
    outline: none;
  }
}

.hidden-element {
  display: none;
}

.index-col {
  background-color: #F8F8F9 !important;
}

.x-data-grid{
  height: 100%;
  user-select: text !important;
}
</style>
