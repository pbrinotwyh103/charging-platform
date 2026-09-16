const palette = {
  cyan: '#38bdf8', blue: '#3b82f6', green: '#22c55e', yellow: '#f59e0b',
  red: '#ef4444', purple: '#8b5cf6', text: '#cbd5e1', strongText: '#f8fafc',
  muted: '#94a3b8', neutral: '#64748b', surface: '#1b2336', elevated: '#272f42', grid: '#475569'
};

const fontFamily = 'PingFang SC, Microsoft YaHei, Noto Sans CJK SC, Segoe UI, sans-serif';
const reduceMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

const AdviceTable = {
  props: {title: String, description: String, items: {type: Array, default: () => []}},
  methods: {
    severityLabel(value) { return ({severe: '严重', attention: '关注', normal: '普通'})[value] || '未分级'; },
    severityClass(value) { return ({severe: 'danger', attention: 'warning', normal: 'success'})[value] || 'neutral'; },
    formatDateTime(value) { return value ? String(value).replace('T', ' ').replace('Z', '') : '—'; }
  },
  template: `
    <article class="panel advice-section">
      <div class="panel-title"><div><h2>{{ title }}</h2><p class="chart-summary">{{ description }}</p></div><span>{{ items.length }} 条</span></div>
      <div class="table-wrap">
        <table>
          <caption>{{ title }}明细</caption>
          <thead><tr><th>级别</th><th>站点</th><th>判断依据</th><th>模型版本</th><th>生成时间</th></tr></thead>
          <tbody v-if="items.length"><tr v-for="item in items" :key="item.adviceId || item.stationId">
            <td><span :class="['status-badge', severityClass(item.severity)]">{{ severityLabel(item.severity) }}</span></td>
            <td class="primary-cell">{{ item.stationName || '未命名站点' }}</td>
            <td><ul class="reason-list"><li v-for="reason in (item.reasons || [])" :key="reason">{{ reason }}</li><li v-if="!(item.reasons || []).length">—</li></ul></td>
            <td>{{ item.modelVersion || '—' }}</td><td>{{ formatDateTime(item.generatedAt) }}</td>
          </tr></tbody>
          <tbody v-else><tr><td colspan="5" class="table-empty">暂无{{ title }}</td></tr></tbody>
        </table>
      </div>
    </article>`
};

const api = async (path) => {
  const response = await fetch(path);
  if (!response.ok) throw new Error(`${path}: ${response.status}`);
  const body = await response.json();
  return {data: Object.prototype.hasOwnProperty.call(body, 'data') ? body.data : body, meta: body.meta || {}};
};

const dashboardApp = Vue.createApp({
  data() {
    return {
      overview: {}, revenue: [], stationRank: [], hourly: [], pileStatus: [],
      districts: [], alarms: [], quality: {}, predictions: [], model: {}, horizon: 1,
      regulator: {}, modelComparison: {}, drift: {}, scheduling: [], maintenance: [], expansion: [],
      currentTime: '', charts: [], view: 'operations', state: 'loading', stale: false,
      generatedAt: '', errorMessage: '', refreshInFlight: false, refreshTimer: null, clockTimer: null
    };
  },
  computed: {
    currentPredictions() {
      return this.predictions.filter(item => Number(item.horizonHours ?? item.forecast_horizon_hours) === this.horizon);
    }
  },
  methods: {
    number(value) { return Number(value || 0).toLocaleString('zh-CN', {maximumFractionDigits: 2}); },
    integer(value) { return Math.round(Number(value || 0)).toLocaleString('zh-CN'); },
    money(value) { return Number(value || 0).toLocaleString('zh-CN', {minimumFractionDigits: 2, maximumFractionDigits: 2}); },
    pretty(value) { return JSON.stringify(value, null, 2); },
    list(value) { return Array.isArray(value) ? value : []; },
    entries(value) { return Object.entries(value || {}); },
    formatDateTime(value) { return value ? String(value).replace('T', ' ').replace('Z', '') : '—'; },
    severityLabel(value) { return ({severe: '严重', attention: '关注', normal: '正常'})[value] || '暂无评估'; },
    severityClass(value) { return ({severe: 'danger', attention: 'warning', normal: 'success'})[value] || 'neutral'; },
    interval(item) {
      const lower = item.lowerBound ?? item.lower_bound;
      const upper = item.upperBound ?? item.upper_bound;
      return lower == null || upper == null ? '—' : `${this.number(lower)}–${this.number(upper)}`;
    },
    adviceCount(severity) {
      return [this.scheduling, this.maintenance, this.expansion]
        .flatMap(value => this.list(value)).filter(item => item.severity === severity).length;
    },
    handleViewTabKeydown(event) {
      const keys = ['ArrowRight', 'ArrowLeft', 'Home', 'End'];
      if (!keys.includes(event.key)) return;
      const views = ['operations', 'regulator', 'tenant'];
      const current = views.indexOf(this.view);
      const next = event.key === 'Home' ? 0
        : event.key === 'End' ? views.length - 1
        : event.key === 'ArrowRight' ? (current + 1) % views.length
        : (current - 1 + views.length) % views.length;
      event.preventDefault();
      this.view = views[next];
      this.$nextTick(() => {
        const tab = document.querySelector(`[role="tab"][data-view="${this.view}"]`);
        if (tab) tab.focus();
      });
    },
    chart(refName) {
      const element = this.$refs[refName];
      const chart = echarts.getInstanceByDom(element) || echarts.init(element);
      if (!this.charts.includes(chart)) this.charts.push(chart);
      return chart;
    },
    baseOption() {
      return {
        animationDuration: reduceMotion ? 0 : 180,
        animationDurationUpdate: reduceMotion ? 0 : 120,
        textStyle: {color: palette.text, fontFamily, fontSize: 12},
        tooltip: {
          trigger: 'axis', confine: true,
          axisPointer: {type: 'line', lineStyle: {color: palette.muted, type: 'dashed'}},
          backgroundColor: palette.elevated, borderColor: palette.grid, borderWidth: 1,
          padding: [8, 12], textStyle: {color: palette.strongText, fontFamily, fontSize: 12}
        },
        grid: {left: 54, right: 28, top: 48, bottom: 42},
        xAxis: {axisLine: {lineStyle: {color: palette.grid}}, axisTick: {show: false}, axisLabel: {color: palette.muted, fontFamily}},
        yAxis: {axisLine: {show: false}, axisTick: {show: false}, splitLine: {lineStyle: {color: palette.grid}}, axisLabel: {color: palette.muted, fontFamily}}
      };
    },
    renderCharts() {
      this.chart('revenueChart').setOption({
        ...this.baseOption(), legend: {data: ['营收（元）', '订单数'], textStyle: {color: palette.text}},
        xAxis: {...this.baseOption().xAxis, type: 'category', data: this.revenue.map(x => x.date.slice(5))},
        yAxis: [{...this.baseOption().yAxis, type: 'value'}, {...this.baseOption().yAxis, type: 'value', splitLine: {show: false}}],
        series: [
          {name: '营收（元）', type: 'line', smooth: true, symbol: 'none', lineStyle: {width: 3, color: palette.cyan}, areaStyle: {color: 'rgba(54,225,233,.12)'}, data: this.revenue.map(x => x.revenue_yuan)},
          {name: '订单数', type: 'bar', yAxisIndex: 1, barMaxWidth: 16, itemStyle: {color: 'rgba(75,136,255,.45)', borderRadius: [4,4,0,0]}, data: this.revenue.map(x => x.orders)}
        ]
      });
      this.chart('pileChart').setOption({
        tooltip: {trigger: 'item'}, legend: {bottom: 0, textStyle: {color: palette.text}},
        series: [{type: 'pie', radius: ['48%', '72%'], center: ['50%', '44%'], label: {color: palette.text, formatter: '{b}\n{d}%'},
          data: this.pileStatus.map((x, i) => ({name: x.status, value: x.count, itemStyle: {color: [palette.green, palette.blue, palette.yellow, palette.red, palette.neutral, palette.purple][i % 6]}}))}]
      });
      this.chart('hourChart').setOption({
        ...this.baseOption(), xAxis: {...this.baseOption().xAxis, type: 'category', data: this.hourly.map(x => `${x.hour}:00`)},
        yAxis: {...this.baseOption().yAxis, type: 'value'},
        series: [{type: 'line', smooth: true, symbolSize: 6, itemStyle: {color: palette.green}, lineStyle: {width: 3}, areaStyle: {color: 'rgba(62,230,168,.14)'}, data: this.hourly.map(x => x.avg_sessions)}]
      });
      this.chart('stationChart').setOption({
        ...this.baseOption(), grid: {left: 118, right: 36, top: 24, bottom: 28},
        xAxis: {...this.baseOption().xAxis, type: 'value'},
        yAxis: {...this.baseOption().yAxis, type: 'category', inverse: true, data: this.stationRank.map(x => x.station_name)},
        series: [{type: 'bar', barWidth: 13, label: {show: true, position: 'right', color: palette.text}, itemStyle: {color: palette.blue, borderRadius: [0, 3, 3, 0]}, data: this.stationRank.map(x => x.revenue_yuan)}]
      });
      const districtRows = [...this.districts].sort((a, b) => Number(b.revenue_yuan) - Number(a.revenue_yuan));
      this.chart('districtChart').setOption({
        ...this.baseOption(), grid: {left: 72, right: 36, top: 24, bottom: 28},
        xAxis: {...this.baseOption().xAxis, type: 'value'},
        yAxis: {...this.baseOption().yAxis, type: 'category', inverse: true, data: districtRows.map(x => x.district)},
        series: [{type: 'bar', barWidth: 14, label: {show: true, position: 'right', color: palette.text}, itemStyle: {color: palette.cyan, borderRadius: [0, 3, 3, 0]}, data: districtRows.map(x => x.revenue_yuan)}]
      });
      const categories = {};
      (this.quality.rule_summary || []).forEach(x => categories[x.category] = (categories[x.category] || 0) + Number(x.count));
      this.chart('qualityChart').setOption({
        ...this.baseOption(), grid: {left: 80, right: 22, top: 22, bottom: 28},
        xAxis: {...this.baseOption().xAxis, type: 'value'},
        yAxis: {...this.baseOption().yAxis, type: 'category', data: Object.keys(categories)},
        series: [{type: 'bar', data: Object.values(categories), itemStyle: {color: palette.red, borderRadius: 6}, barWidth: 16}]
      });
      this.chart('alarmChart').setOption({
        tooltip: {trigger: 'item'}, legend: {bottom: 0, textStyle: {color: palette.text}},
        series: [{type: 'pie', radius: ['42%', '70%'], label: {color: palette.text},
          data: this.alarms.map(x => ({name: x.level || x.status || x.type, value: x.count}))}]
      }, true);
      this.predictionChart = this.chart('predictionChart');
      this.renderPrediction();
    },
    renderPrediction() {
      if (!this.predictionChart) return;
      const rows = this.currentPredictions.slice(0, 10);
      this.predictionChart.setOption({
        ...this.baseOption(), grid: {left: 118, right: 38, top: 28, bottom: 30},
        xAxis: {...this.baseOption().xAxis, type: 'value', name: '预测会话数'},
        yAxis: {...this.baseOption().yAxis, type: 'category', inverse: true, data: rows.map(x => x.stationName ?? x.station_name)},
        tooltip: {...this.baseOption().tooltip, formatter: params => {
          const index = params[0]?.dataIndex ?? 0;
          const row = rows[index] || {};
          const name = row.stationName ?? row.station_name ?? '未命名站点';
          const value = row.predictedSessions ?? row.predicted_sessions ?? 0;
          const lowerBound = row.lowerBound ?? row.lower_bound;
          const upperBound = row.upperBound ?? row.upper_bound;
          const range = lowerBound == null || upperBound == null ? '—' : `${lowerBound}–${upperBound}`;
          return `${name}<br>预测会话：${value}<br>模型区间：${range}`;
        }},
        series: [{type: 'bar', data: rows.map(x => x.predictedSessions ?? x.predicted_sessions), barWidth: 14,
          label: {show: true, position: 'right', color: palette.text}, itemStyle: {color: palette.purple, borderRadius: [0, 3, 3, 0]}}]
      }, true);
    },
    async refresh() {
      if (this.refreshInFlight) return;
      this.refreshInFlight = true;
      if (!this.overview || !Object.keys(this.overview).length) this.state = 'loading';
      try {
        const paths = [
          '/api/overview', '/api/revenue-trend', '/api/station-rank', '/api/hourly-load',
          '/api/pile-status', '/api/district-metrics', '/api/alarm-distribution', '/api/quality',
          '/api/predictions', '/api/model-metrics', '/api/regulator', '/api/model-comparison',
          '/api/drift', '/api/scheduling', '/api/maintenance', '/api/expansion'
        ];
        const results = await Promise.all(paths.map(api));
        [this.overview, this.revenue, this.stationRank, this.hourly, this.pileStatus,
          this.districts, this.alarms, this.quality, this.predictions, this.model,
          this.regulator, this.modelComparison, this.drift, this.scheduling,
          this.maintenance, this.expansion] = results.map(result => result.data);
        const metas = results.map(result => result.meta);
        this.stale = metas.some(meta => meta.stale);
        this.generatedAt = (metas.find(meta => meta.generatedAt) || {}).generatedAt || '';
        const hasData = results.some(result => Array.isArray(result.data) ? result.data.length : Object.keys(result.data || {}).length);
        this.state = hasData ? 'ready' : 'empty';
        this.errorMessage = '';
        await this.$nextTick();
        this.renderCharts();
      } catch (error) {
        this.errorMessage = '分析服务暂不可用';
        this.state = 'error';
        console.error('dashboard refresh failed', error.message);
      } finally {
        this.refreshInFlight = false;
      }
    }
  },
  async mounted() {
    const updateTime = () => this.currentTime = new Date().toLocaleString('zh-CN', {hour12: false});
    updateTime(); this.clockTimer = setInterval(updateTime, 1000);
    this.resizeHandler = () => this.charts.forEach(chart => chart.resize());
    window.addEventListener('resize', this.resizeHandler);
    await this.refresh();
    this.refreshTimer = setInterval(() => this.refresh(), 60000);
  },
  beforeUnmount() {
    clearInterval(this.clockTimer); clearInterval(this.refreshTimer);
    window.removeEventListener('resize', this.resizeHandler);
    this.charts.forEach(chart => chart.dispose());
  }
});

dashboardApp.component('advice-table', AdviceTable);
dashboardApp.mount('#app');
