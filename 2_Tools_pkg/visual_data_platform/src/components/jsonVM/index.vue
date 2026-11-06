<template>
  <div style="padding: 0 0px">
    <div
      v-for="(item,key) in schema"
      :key="key"
    >
      <template v-if="item.type==='readFile'">
        <a-row
          type="flex"
          align="middle"
          style="margin: 5px 0;"
        >
          <span
            v-if="item.required!==false"
            style="color: red;"
          >*</span>{{ item.title }}:
          <span class="file-name-display">
            {{ value[key].split('\\').pop() || '未选择' }}
          </span>
          <a-button
            type="link"
            icon="file-add"
            @click="callbacks['openFile'](value,key,item.options)"
          />
        </a-row>
      </template>
      <template v-if="item.type==='saveFile'">
        <a-row
          type="flex"
          align="middle"
          style="margin: 5px 0;"
        >
          <span
            v-if="item.required!==false"
            style="color: red;"
          >*</span>{{ item.title }}:
          <span class="file-name-display">
            {{ value[key].split('\\').pop() || '未选择' }}
          </span>
          <a-button
            type="link"
            icon="save"
            @click="callbacks['saveFile'](value,key,item.options)"
          />
        </a-row>
      </template>
      <template v-if="item.type==='enum'">
        <a-row
          type="flex"
          align="middle"
          style="margin: 5px 0;"
        >
          <a-col :flex="item.titleFlex || '80px'">
            <span
              v-if="item.required!==false"
              style="color: red;"
            >*</span>{{ item.title }}:
          </a-col>
          <a-col
            :style="{
              flex: '1 1 '+(item.valueFlex||0),
              textAlign: 'right'
            }"
          >
            <a-select
              v-model="value[key]"
              v-bind="{
                size:'default',
                style:{
                  width:'100%'
                },
                placeholder: item.title,
                options:item.enum.map(p=>{
                  return {value:p, label:p}
                }),
                ...item.bind
              }"
            />
          </a-col>
        </a-row>
      </template>
      <template v-if="item.type==='string'">
        <a-row
          type="flex"
          align="middle"
          style="margin: 5px 0;"
        >
          <a-col :flex="item.titleFlex || '80px'">
            <span
              v-if="item.required!==false"
              style="color: red;"
            >*</span>{{ item.title }}:
          </a-col>
          <a-col
            :style="{
              flex: '1 1 '+(item.valueFlex||0),
              textAlign: 'right'
            }"
          >
            <a-input
              v-model="value[key]"
              v-bind="{
                style:{
                  width: '100%'
                },
                placeholder: item.title,
                size:'default',
                ...item.bind
              }"
            />
          </a-col>
        </a-row>
      </template>
      <template v-if="item.type==='textarea'">
        <a-row
          type="flex"
          align="middle"
          style="margin: 5px 0;"
        >
          <a-col :flex="item.titleFlex || '80px'">
            <span
              v-if="item.required!==false"
              style="color: red;"
            >*</span>{{ item.title }}:
          </a-col>
          <a-col
            :style="{
              flex: '1 1 '+(item.valueFlex||0),
              textAlign: 'right'
            }"
          >
            <a-textarea
              v-model="value[key]"
              v-bind="{
                style:{
                  width: '100%'
                },
                placeholder: item.title,
                size:'default',
                ...item.bind
              }"
            />
          </a-col>
        </a-row>
      </template>
      <template v-if="item.type==='integer'">
        <a-row
          type="flex"
          align="middle"
          style="margin: 5px 0;"
        >
          <a-col :flex="item.titleFlex || '80px'">
            <span
              v-if="item.required!==false"
              style="color: red;"
            >*</span>{{ item.title }}:
          </a-col>
          <a-col
            :style="{
              flex: '1 1 '+(item.valueFlex||0),
              textAlign: 'right'
            }"
          >
            <a-input-number
              v-model="value[key]"
              v-bind="{
                style:{
                  width: '100%'
                },
                size:'default',
                placeholder: item.title,
                ...item.bind
              }"
            />
          </a-col>
        </a-row>
      </template>
      <template v-if="item.type==='number'">
        <a-row
          type="flex"
          align="middle"
          style="margin: 5px 0;"
        >
          <a-col :flex="item.titleFlex || '80px'">
            <span
              v-if="item.required!==false"
              style="color: red;"
            >*</span>{{ item.title }}:
          </a-col>
          <a-col
            :style="{
              flex: '1 1 '+(item.valueFlex||0),
              textAlign: 'right'
            }"
          >
            <a-input-number
              v-model="value[key]"
              v-bind="{
                style:{
                  width: '100%'
                },
                size:'default',
                placeholder: item.title,
                step:0.1,
                ...item.bind
              }"
            />
          </a-col>
        </a-row>
      </template>
      <template v-if="item.type==='rowSelect'">
        <a-row
          :gutter="[16]"
          style="margin:0 1px 8px 1px;  background: #eee"
        >
          <a-col :span="7">
            条件列
          </a-col>
          <a-col :span="5">
            比较符
          </a-col>
          <a-col :span="12">
            比较数值
          </a-col>
        </a-row>
        <a-row
          v-for="(condition,index) in value[key]"
          :key="index"
          :gutter="[10]"
          style="margin-bottom:8px;"
        >
          <a-col :span="7">
            <a-select
              v-model="condition.key"
              v-bind="{
                size:'default',
                style:{
                  width:'100%'
                },
                placeholder: '条件列',
                options:[
                  {key:'index', title:'行号'},
                  ...item.items.map(p=>{
                    return {key:p, title:p}
                  })
                ]
              }"
            />
          </a-col>
          <a-col :span="5">
            <a-select
              v-model="condition.operator"
              v-bind="{
                size:'default',
                style:{
                  width:'100%'
                },
                placeholder: '筛选条件',
                options:[
                  {key:'>',title:'>'},
                  {key:'<',title:'<'},
                  {key:'>=',title:'>='},
                  {key:'<=',title:'<='},
                  {key:'=',title:'='},
                  {key:'!=',title:'!='},
                ]
              }"
            />
          </a-col>
          <a-col :span="10">
            <a-input
              v-model="condition.value"
              placeholder="数值"
            />
          </a-col>
          <a-col :span="2">
            <a-icon
              type="delete"
              style="cursor:point;"
              @click="value[key].splice(index,1)"
            />
          </a-col>
        </a-row>
        <a-button
          block
          type="dashed"
          @click="value[key].push({key:'index', value:'0',operator:'>='})"
        >
          <a-icon type="plus" />新建条件
        </a-button>
      </template>
      <template v-if="item.type==='array'">
        <a-transfer
          :data-source="item.items.map(p=>{
            return {key:p, title:p}
          })"
          show-search
          :list-style="{
            width: 'calc(50% - 20px)',
            height: '360px',
          }"
          :target-keys="value[key]"
          :render="item => item.title"
          @change="setValue(value,key,$event)"
        />
      </template>
      <template v-if="item.type==='boolean'">
        <a-checkbox
          :checked="value[key]"
          @change="setValue(value,key,$event.target.checked)"
        >
          {{ item.title }}
        </a-checkbox>
      </template>
      <template v-if="item.type==='color'">
        <a-row
          type="flex"
          align="middle"
          style="margin: 5px 0;"
        >
          <a-col :flex="item.titleFlex || '80px'">
            <span
              v-if="item.required!==false"
              style="color: red;"
            >*</span>{{ item.title }}:
          </a-col>
          <a-col
            :style="{
              flex: '1 1 '+(item.valueFlex||0),
              textAlign: 'right'
            }"
          >
            <ColorPicker
              :default-color="value[key]"
              @change="setValue(value,key,$event)"
            />
          </a-col>
        </a-row>
      </template>
      <template v-if="item.type==='object'">
        <div class="groupbox">
          <span class="grouptitle">{{ item.title }}</span>
          <json-v-m
            :value="value[key]"
            :schema="item.properties"
            :callbacks="callbacks"
          />
        </div>
      </template>
    </div>
    <a-empty
      v-if="Object.keys(schema).length===0"
      description="无配置项"
    />
  </div>
</template>

<script>
import ColorPicker from '../colorPicker'

export default {
  name: 'JsonVM',

  components: {
    ColorPicker
  },

  props: {
    value: {
      type: Object,
      required: true
    },
    schema: {
      type: Object,
      required: true
    },
    callbacks: {
      type: Object,
      default: () => {
        return {}
      }
    }
  },
  methods: {
    setValue (object, key, value) {
      this.$set(object, key, value)
    }
  }
}
</script>

<style lang="less" scoped>
.file-name-display {
  display:inline-block;
  flex: 1 1 0;
  border: 1px solid #ccc;
  border-radius:5px;
  padding:0 10px;
  overflow: hidden;
  white-space: nowrap;
  text-overflow: ellipsis;
}

.groupbox {
  padding:10px;
  margin: 10px 0;
	width: 100%;
	border:thin solid #ccc;
  padding:10px;
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

