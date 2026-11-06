<template>
  <div
    style="width:280px;height:100%"
  >
    <div class="widget-container">
      <div class="group-list">
        <a-select
          v-model="selectIndex"
          style="width: 100%"
        >
          <a-select-option
            v-for="(item,index) in widgets"
            :key="index"
            :value="index"
            @change="scroll2Top"
          >
            <img
              :src="item.icon"
              width="21"
              height="21"
            >
            {{ item.label }}
          </a-select-option>
        </a-select>
      </div>
      <div
        ref="group"
        class="group-item"
      >
        <a-collapse
          v-if="widgets.length>0"
          :bordered="false"
        >
          <template #expandIcon="props">
            <a-icon
              type="caret-right"
              :rotate="props.isActive ? 90 : 0"
            />
          </template>
          <template v-for="(group,index) in widgets[selectIndex].components">
            <a-collapse-panel
              v-if="getModels(group.components).length>0"
              :key="group.label+index+group.order"
              :header="`${group.label} (${getModels(group.components).length})`"
              :style="customStyle"
            >
              <a-list
                :grid="{ gutter: 2, column: 3 }"
                :data-source="getModels(group.components)"
              >
                <a-list-item
                  slot="renderItem"
                  slot-scope="el"
                  @click.right="showMenuOption(el,$event)"
                >
                  <drag
                    :key="el.modulePath + el.moduleKey"
                    class="item-render"
                    :transfer-data="el"
                    @mouseover.native="hoverItem=el"
                    @mouseleave.native="hoverItem=null"
                  >
                    <img
                      :src="el.icon"
                      width="35"
                      height="35"
                    >
                    <span class="widget-label">{{ el.label }}</span>
                  </drag>
                </a-list-item>
              </a-list>
            </a-collapse-panel>
          </template>
        </a-collapse>
      </div>
    </div>
    <div
      class="widget-info"
      v-html="nodeInfo"
    />
  </div>
</template>

<script>
import vuex from '@/vuex'
import { saveFile } from '@/utils/file'

export default {

  props: {
    widgets: {
      type: Array, default: () => {
        return []
      }
    },
    selectedNode: {
      type: Object, default: () => { return {} }
    }
  },

  data () {
    return {
      // 组件列表样式
      customStyle: {
        background: '#EFEFEF',
        borderBottom: '1px solid #DCDCDC',
        margin: '0',
        padding: '0 5px',
        fontSize: '13px'
      },
      selectIndex: 0,
      hoverItem: null
    }
  },

  computed: {
    nodeInfo () {
      const model = this.hoverItem ? this.hoverItem : (this.selectedNode && !this.selectedNode.destroyed) ? this.selectedNode.getModel() : null

      if (model) {
        let info = `<span style="color:#FFA840;">${model.shortDescription || model.label}</span>`
        if (model.inAnchor.length > 0) {
          info += '<br><div style="width:100%;height:1px;background:grey"/>'
          info += `输入:`
          for (const anchor of model.inAnchor) {
            info += `<br><span style="margin-left:10px;">·类型: ${anchor.type}, 描述: ${anchor.label}</span>`
          }
        }
        if (model.outAnchor.length > 0) {
          info += '<br><div style="width:100%;height:1px;background:grey"/>'
          info += `输出:`
          for (const anchor of model.outAnchor) {
            info += `<br><span style="margin-left:10px;">·类型: ${anchor.type}, 描述: ${anchor.label}</span>`
          }
        }
        return info
      } else {
        return '选择模块查看信息'
      }
    }
  },

  methods: {
    getModel (model) {
      const { env } = vuex
      const _model = { ...model.default }
      Object.assign(_model, model[env] || {})
      return _model
    },
    getModels (models) {
      const _modles = []
      for (const model of models) {
        const _model = this.getModel(model)
        if (_model.visible !== false) {
          _modles.push(_model)
        }
      }
      return _modles
    },
    scroll2Top () {
      this.$refs.group.scrollTop = 0
    },
    showMenuOption (el, event) {
      if (el.isCollect) {
        this.$contextmenu({
          items: [
            {
              label: '删除',
              icon: 'iconfont icon-delete',
              onClick: () => {
                const index = vuex.collect.components.findIndex(p => p.default.moduleKey === el.moduleKey)
                vuex.collect.components.splice(index, 1)
                saveFile(vuex.collect.file, 'module.exports = ' + JSON.stringify(vuex.collect.components, null, 2))
              }
            }
          ],
          event,
          zIndex: 99999,
          minWidth: 200
        })
      }
    }
  }
}
</script>

<style lang="less" scoped>
.widget-container {
  display: flex;
  flex-direction: column;
  height: calc(100% - 220px);
  user-select: none;

  .group-list {
    width: 100%;
    background: #DDD;
  }

  .group-item {
    flex: 1;
    width: 100%;
    overflow: auto;
    background: #EAEAEA;

    ul {
      padding: 0;
      margin-bottom: 0;
    }

    .item-render {
      padding: 5px;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;

      &:last-child {
        margin-bottom: 0;
      }

      &:hover {
        background: white;
        border-radius: 5px;
        cursor: grab;
      }
    }
  }

  .add-more {
    width: 100%;
    color: #2972CB;
    text-align: center;
    cursor: pointer;
  }
}

.widget-label {
  width: 100%;
  text-align: center;
  font-size:12px;
  overflow: hidden;
  white-space: nowrap;
  text-overflow: ellipsis;
}

.widget-info {
  width: 100%;
  height: 220px;
  padding: 10px;
  overflow: auto;
  background-color: #FCFCFC;
  border: 2px solid #EAEAEA;
}
</style>
