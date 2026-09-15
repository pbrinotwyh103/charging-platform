const palette = {
  cyan: '#36e1e9', blue: '#4b88ff', green: '#3ee6a8', yellow: '#ffd166',
  red: '#ff6b7a', purple: '#9d7bff', text: '#d8e7ff', grid: 'rgba(110,150,210,.14)'
};

const api = async (path) => {
  const response = await fetch(path);
  if (!response.ok) throw new Error(`${path}: ${response.status}`);
  const body = await response.json();
  return {data: Object.prototype.hasOwnProperty.call(body, 'data') ? body.data : body, meta: body.meta || {}};
};

Vue.createApp({
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
    chart(refName) {
      const element = this.$refs[refName];
      const chart = echarts.getInstanceByDom(element) || echarts.init(element);
      if (!this.charts.includes(chart)) this.charts.push(chart);
      return chart;
    },
    baseOption() {
      return {
        textStyle: {color: palette.text, fontFamily: 'Microsoft YaHei, sans-serif'},
        tooltip: {trigger: 'axis', backgroundColor: 'rgba(7,20,45,.96)', borderColor: palette.cyan, textStyle: {color: '#fff'}},
        grid: {left: 54, right: 28, top: 48, bottom: 42},
        xAxis: {axisLine: {lineStyle: {color: palette.grid}}, axisLabel: {color: '#8fa8cd'}},
        yAxis: {splitLine: {lineStyle: {color: palette.grid}}, axisLabel: {color: '#8fa8cd'}}
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
          data: this.pileStatus.map((x, i) => ({name: x.status, value: x.count, itemStyle: {color: [palette.green, palette.blue, palette.yellow, palette.red, '#64748b', palette.purple][i % 6]}}))}]
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
        series: [{type: 'bar', barWidth: 13, itemStyle: {color: new echarts.graphic.LinearGradient(0,0,1,0,[{offset:0,color:palette.blue},{offset:1,color:palette.cyan}]), borderRadius: 7}, data: this.stationRank.map(x => x.revenue_yuan)}]
      });
      this.chart('districtChart').setOption({
        tooltip: {trigger: 'item'}, series: [{type: 'pie', roseType: 'radius', radius: ['22%', '72%'], center: ['50%', '50%'],
          label: {color: palette.text}, data: this.districts.map(x => ({name: x.district, value: x.revenue_yuan}))}]
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
        series: [{type: 'bar', data: rows.map(x => x.predictedSessions ?? x.predicted_sessions), barWidth: 14,
          itemStyle: {color: new echarts.graphic.LinearGradient(0,0,1,0,[{offset:0,color:palette.purple},{offset:1,color:palette.red}]), borderRadius: 7}}]
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
}).mount('#app');
