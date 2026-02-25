/*
***************************************************************************  
**  Program  : graph.js, part of OTGW-firmware project
**  Version  : v1.1.0-beta
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

// Configuration constants
const UPDATE_INTERVAL_MS = 2000; // Update chart every 2 seconds to reduce load

// Helper to detect Dallas sensor entries via the explicit "type":"dallas"
// field added by the firmware API.  Accepts an API entry object.
function isDallasAddress(entry) {
  return entry != null && entry.type === 'dallas';
}

var OTGraph = {
    chart: null,
    data: {},
    pendingData: {}, // Track new data points since last chart update
    maxPoints: 864000, // Buffer to hold 24h of data at 10 msgs/sec
    timeWindow: 3600 * 1000, // Default 1 Hour in milliseconds
    running: false,
    updateTimer: null,
    captureTimer: null,
    currentTheme: 'light',
    lastUpdate: 0,
    updateInterval: UPDATE_INTERVAL_MS,
    disconnectMarkers: [], // Track disconnect/reconnect events: [{time: timestamp, type: 'disconnect'|'reconnect'}]
    resizeHandler: null, // Store resize handler reference for cleanup
    initialized: false, // Track if already initialized to prevent duplicate event listeners
    
    // Store DOM event handler references for cleanup
    timeWindowHandler: null,
    screenshotHandler: null,
    autoScreenshotHandler: null,
    exportHandler: null,
    autoExportHandler: null,

    // Define palettes
    palettes: {
        light: {
            flame: 'red',
            dhwMode: 'blue',
            chMode: 'green',
            mod: 'black',
            ctrlSp: 'grey',
            boiler: 'red',
            return: 'blue',
            roomSp: 'darkcyan', 
            room: 'magenta',
            outside: 'green'
        },
        dark: {
            flame: '#ff4d4f', 
            dhwMode: '#40a9ff', 
            chMode: '#73d13d', 
            mod: '#ffffff',     
            ctrlSp: '#bfbfbf',  
            boiler: '#ff7875',  
            return: '#69c0ff',  
            roomSp: 'cyan',     
            room: '#ff85c0',    
            outside: '#95de64'  
        }
    },

    // Color palettes for Dallas temperature sensors (up to 16 sensors)
    sensorColorPalettes: {
        light: [
            '#FF6B6B', '#4ECDC4', '#45B7D1', '#FFA07A', '#98D8C8',
            '#F7DC6F', '#BB8FCE', '#85C1E2', '#F8B195', '#C06C84',
            '#6C5B7B', '#355C7D', '#2A9D8F', '#E76F51', '#F4A261',
            '#E9C46A'
        ],
        dark: [
            '#FF8787', '#5FE3D9', '#5BC8E8', '#FFB59A', '#ADE8D8',
            '#FFE66D', '#D1A5E6', '#A0D6F2', '#FFD1B5', '#D688A4',
            '#8677A1', '#4A7BA7', '#3EBFB0', '#FF8C71', '#FFB881',
            '#FFD78A'
        ]
    },
    
    // Track discovered temperature sensors
    detectedSensors: [],
    sensorAddressToId: {}, // Map sensor hex address to internal ID

    // 5 separate grids/graphs
    // 0: Flame (Digital)
    // 1: DHW Mode (Digital)
    // 2: CH Mode (Digital)
    // 3: Modulation (Analog 0-100)
    // 4: Temperature (Analog)
    // Base series configuration (sensors are added dynamically)
    seriesConfig: [
        { id: 'flame',   label: 'Flame',    gridIndex: 0, type: 'line', step: 'start', areaStyle: { opacity: 0.3 }, large: true, sampling: 'lttb' },
        { id: 'dhwMode', label: 'DHW Mode', gridIndex: 1, type: 'line', step: 'start', areaStyle: { opacity: 0.3 }, large: true, sampling: 'lttb' },
        { id: 'chMode',  label: 'CH Mode',  gridIndex: 2, type: 'line', step: 'start', areaStyle: { opacity: 0.3 }, large: true, sampling: 'lttb' },
        { id: 'mod',     label: 'Modulation (%)',   gridIndex: 3, type: 'line', step: false, large: true, sampling: 'lttb' },
        { id: 'ctrlSp',  label: 'Control SetPoint',       gridIndex: 4, type: 'line', step: false, large: true, sampling: 'lttb' },
        { id: 'boiler',  label: 'Boiler Temp',      gridIndex: 4, type: 'line', step: false, large: true, sampling: 'lttb' },
        { id: 'return',  label: 'Return Temp',      gridIndex: 4, type: 'line', step: false, large: true, sampling: 'lttb' },
        { id: 'roomSp',  label: 'Room SetPoint',          gridIndex: 4, type: 'line', step: false, large: true, sampling: 'lttb' },
        { id: 'room',    label: 'Room Temp',        gridIndex: 4, type: 'line', step: false, large: true, sampling: 'lttb' },
        { id: 'outside', label: 'Outside Temp',     gridIndex: 4, type: 'line', step: false, large: true, sampling: 'lttb' }
    ],

    init: function() {
        console.log("OTGraph init (ECharts)");
        
        // Prevent duplicate initialization
        if (this.initialized) {
            console.log("OTGraph already initialized, skipping");
            return;
        }
        
        var container = document.getElementById('otGraphCanvas');
        if (!container) return; // Wait for DOM

        // Determine theme
        this.currentTheme = 'light';
        try {
            if (localStorage.getItem('theme') === 'dark') this.currentTheme = 'dark';
        } catch(e) {}

        if (typeof echarts === 'undefined') {
            console.error("ECharts library not loaded");
            container.innerHTML = '' +
                '<div class="graph-error">' +
                    '<h3>Graph unavailable</h3>' +
                    '<p>The ECharts graph library could not be loaded. The graph feature requires access to the ECharts CDN on the internet.</p>' +
                    '<ul>' +
                        '<li>Check that this device has internet connectivity and is allowed to access external CDNs.</li>' +
                        '<li>If you are running in an isolated network, refer to the OTGW-firmware documentation for using a locally hosted copy of ECharts.</li>' +
                        '<li>For more technical details, open your browser developer console (F12) and look for network or script loading errors.</li>' +
                    '</ul>' +
                    '<button type="button" onclick="window.location.reload()">Retry loading graph</button>' +
                '</div>';
            return;
        }

        this.chart = echarts.init(container, this.currentTheme);
        
        // Bind settings - store handler references for cleanup
        var timeSelect = document.getElementById('graphTimeWindow');
        if (timeSelect) {
            this.timeWindowHandler = (e) => {
                this.setTimeWindow(parseInt(e.target.value, 10));
            };
            timeSelect.addEventListener('change', this.timeWindowHandler);
            // Set initial value directly to avoid calling updateChart before init
            var initialMinutes = parseInt(timeSelect.value, 10);
            if (initialMinutes && !isNaN(initialMinutes)) {
                this.timeWindow = initialMinutes * 60 * 1000;
            }
        }
        
        var btnShot = document.getElementById('btnGraphScreenshot');
        if (btnShot) {
            this.screenshotHandler = () => {
                this.screenshot(false);
            };
            btnShot.addEventListener('click', this.screenshotHandler);
        }
        
        var chkAutoShot = document.getElementById('chkAutoScreenshot');
        if (chkAutoShot) {
            this.autoScreenshotHandler = (e) => {
                 this.toggleAutoScreenshot(e.target.checked);
                 if (typeof saveUISetting === 'function') saveUISetting('ui_autoscreenshot', e.target.checked);
            };
            chkAutoShot.addEventListener('change', this.autoScreenshotHandler);
        }
        
        var btnExport = document.getElementById('btnGraphExport');
        if (btnExport) {
            this.exportHandler = () => {
                this.exportData(false);
            };
            btnExport.addEventListener('click', this.exportHandler);
        }

        var chkAutoExport = document.getElementById('chkAutoExport');
        if (chkAutoExport) {
            this.autoExportHandler = (e) => {
                 this.toggleAutoExport(e.target.checked);
                 if (typeof saveUISetting === 'function') saveUISetting('ui_autoexport', e.target.checked);
            };
            chkAutoExport.addEventListener('change', this.autoExportHandler);
        }

        // Initialize empty data arrays if not present
        this.seriesConfig.forEach(c => {
            if (!this.data[c.id]) this.data[c.id] = [];
            if (!this.pendingData[c.id]) this.pendingData[c.id] = [];
        });

        this.updateOption();
        
        this.running = true;
        
        // Handle resize - store handler reference for potential cleanup
        this.resizeHandler = () => {
            if (this.chart) this.chart.resize();
        };
        window.addEventListener('resize', this.resizeHandler);

        // Throttle updates to chart: use requestAnimationFrame with throttling
        if (this.updateTimer) clearInterval(this.updateTimer);
        this.updateTimer = setInterval(() => {
            requestAnimationFrame(() => this.updateChart());
        }, this.updateInterval);
        
        this.initialized = true;
    },

    getCachedSensorLabel: function(address, labelMap) {
        var cache = labelMap;
        if (!cache && typeof window !== 'undefined') {
            cache = window.dallasLabelsCache;
        }
        if (!cache || !address) return null;
        if (typeof cache[address] !== 'string') return null;
        var label = cache[address].trim();
        return label.length > 0 ? label : null;
    },

    getApiSensorLabel: function(address, apiData) {
        if (!apiData || typeof apiData !== 'object') return null;
        var labelKey = address + '_label';
        if (apiData[labelKey] && apiData[labelKey].value) {
            return apiData[labelKey].value;
        }
        return null;
    },

    getDefaultSensorLabel: function(address, sensorIndex) {
        return 'Sensor ' + (sensorIndex + 1) + ' (' + address + ')';
    },

    resolveSensorLabel: function(address, apiData, sensorIndex, labelMap) {
        var label = this.getCachedSensorLabel(address, labelMap);
        if (!label) {
            label = this.getApiSensorLabel(address, apiData);
        }
        if (!label) {
            label = this.getDefaultSensorLabel(address, sensorIndex);
        }
        return label;
    },

    // Detect and register Dallas temperature sensors from API data
    detectAndRegisterSensors: function(apiData) {
        if (!apiData || typeof apiData !== 'object') return;
        
        var newSensors = [];
        var sensorCount = 0;
        
        // Check if numberofsensors field exists
        if (apiData.numberofsensors && apiData.numberofsensors.value) {
            sensorCount = parseInt(apiData.numberofsensors.value, 10);
        }
        
        if (sensorCount === 0 || isNaN(sensorCount)) return;
        
        // Find all Dallas sensor entries (identified by type field in API response)
        for (var key in apiData) {
            if (!apiData.hasOwnProperty(key)) continue;
            
            // Dallas sensors have type:"dallas" in the API response
            if (isDallasAddress(apiData[key])) {
                
                // Check if this sensor is already registered
                if (!this.sensorAddressToId[key]) {
                    var sensorIndex = this.detectedSensors.length;
                    var sensorId = 'sensor_' + sensorIndex;

                    var sensorLabel = this.resolveSensorLabel(key, apiData, sensorIndex, typeof dallasLabelsCache !== 'undefined' ? dallasLabelsCache : null);
                    
                    // Register the sensor
                    this.sensorAddressToId[key] = sensorId;
                    this.detectedSensors.push({
                        address: key,
                        id: sensorId,
                        index: sensorIndex,
                        label: sensorLabel
                    });
                    
                    // Add to series config
                    this.seriesConfig.push({
                        id: sensorId,
                        label: sensorLabel,
                        gridIndex: 4, // Add to temperature grid
                        type: 'line',
                        step: false,
                        large: true,
                        sampling: 'lttb'
                    });
                    
                    // Initialize data arrays
                    this.data[sensorId] = [];
                    this.pendingData[sensorId] = [];
                    
                    // Add color to palettes
                    var colorIndex = sensorIndex % 16; // Cycle through 16 colors
                    this.palettes.light[sensorId] = this.sensorColorPalettes.light[colorIndex];
                    this.palettes.dark[sensorId] = this.sensorColorPalettes.dark[colorIndex];
                    
                    newSensors.push(sensorLabel);
                    
                    console.log('Graph: Registered temperature sensor:', sensorLabel, 'Address:', key);
                } else {
                    // Sensor already registered, but check if label has changed
                    var sensorId = this.sensorAddressToId[key];

                    // Find the sensor in detectedSensors and update label
                    for (var i = 0; i < this.detectedSensors.length; i++) {
                        if (this.detectedSensors[i].address === key) {
                            var updatedLabel = this.resolveSensorLabel(key, apiData, this.detectedSensors[i].index, typeof dallasLabelsCache !== 'undefined' ? dallasLabelsCache : null);
                            if (this.detectedSensors[i].label !== updatedLabel) {
                                this.detectedSensors[i].label = updatedLabel;

                                // Update seriesConfig label
                                for (var j = 0; j < this.seriesConfig.length; j++) {
                                    if (this.seriesConfig[j].id === sensorId) {
                                        this.seriesConfig[j].label = updatedLabel;
                                        console.log('Graph: Updated sensor label:', updatedLabel, 'Address:', key);
                                        // Trigger chart update
                                        this.updateOption();
                                        break;
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
        
        // If new sensors were added, update the chart
        if (newSensors.length > 0) {
            console.log('Graph: Added', newSensors.length, 'new temperature sensor(s) to graph');
            this.updateOption();
        }
    },

    refreshSensorLabels: function(labelMap) {
        if (!this.detectedSensors || this.detectedSensors.length === 0) return;
        var changed = false;

        for (var i = 0; i < this.detectedSensors.length; i++) {
            var sensor = this.detectedSensors[i];
            var cachedLabel = this.getCachedSensorLabel(sensor.address, labelMap);
            if (cachedLabel && sensor.label !== cachedLabel) {
                sensor.label = cachedLabel;
                var sensorId = this.sensorAddressToId[sensor.address];

                for (var j = 0; j < this.seriesConfig.length; j++) {
                    if (this.seriesConfig[j].id === sensorId) {
                        this.seriesConfig[j].label = cachedLabel;
                        changed = true;
                        break;
                    }
                }
            }
        }

        if (changed) {
            console.log('Graph: Refreshed sensor labels from cache');
            this.updateOption();
        }
    },

    // Process sensor data from API response
    processSensorData: function(apiData, timestamp) {
        if (!apiData || typeof apiData !== 'object') return;
        
        for (var key in apiData) {
            if (!apiData.hasOwnProperty(key)) continue;
            
            // Check if this is a registered sensor
            var sensorId = this.sensorAddressToId[key];
            if (sensorId && apiData[key] && typeof apiData[key].value === 'number') {
                var temp = apiData[key].value;
                
                // Temperature bounds check (reasonable range -50°C to 150°C)
                if (temp >= -50 && temp <= 150) {
                    this.pushData(sensorId, timestamp, temp);
                }
            }
        }
    },

    setTimeWindow: function(minutes) {
        if (!minutes || isNaN(minutes)) return;
        this.timeWindow = minutes * 60 * 1000;
        // console.log("Graph time window set to (ms):", this.timeWindow);
        this.updateChart();
    },
    
    toggleAutoScreenshot: function(enabled) {
        if (this.captureTimer) clearInterval(this.captureTimer);
        
        if (enabled) {
            console.log("Auto-Screenshot enabled (every 15 minutes)");
            this.captureTimer = setInterval(() => {
                this.screenshot(true);
            }, 15 * 60 * 1000); // 15 minutes
        } else {
            console.log("Auto-Screenshot disabled");
        }
    },
    
    toggleAutoExport: function(enabled) {
        if (this.exportTimer) clearInterval(this.exportTimer);
        
        if (enabled) {
            console.log("Auto-Export enabled (every 15 minutes)");
            this.exportTimer = setInterval(() => {
                this.exportData(true);
            }, 15 * 60 * 1000); // 15 minutes
        } else {
            console.log("Auto-Export disabled");
        }
    },

    exportData: function(isAuto) {
        var now = new Date();
        var iso = now.toISOString().replace(/[:.]/g, '-').slice(0, -5); 
        var prefix = isAuto ? 'otgw-data-auto-' : 'otgw-data-';
        var filename = prefix + iso + '.csv';

        // Prepare CSV headers
        var headers = ["Timestamp"];
        var keys = [];
        this.seriesConfig.forEach(c => {
            headers.push(c.label);
            keys.push(c.id);
        });

        // Collect data grouping by timestamp
        var startTime = now.getTime() - this.timeWindow;
        var dataMap = new Map();

        this.seriesConfig.forEach(c => {
             if (this.data[c.id]) {
                 this.data[c.id].forEach(pt => {
                     // pt.value[0] can be Date object or timestamp number
                     var t = (pt.value[0] instanceof Date) ? pt.value[0].getTime() : pt.value[0];
                     if (t >= startTime) {
                         if (!dataMap.has(t)) dataMap.set(t, {});
                         dataMap.get(t)[c.id] = pt.value[1];
                     }
                 });
             }
        });

        // Initialize last known values with the most recent data point before startTime
        var lastValues = {};
        this.seriesConfig.forEach(c => {
             if (this.data[c.id] && this.data[c.id].length > 0) {
                 var seriesData = this.data[c.id];
                 for (var i = seriesData.length - 1; i >= 0; i--) {
                     var pt = seriesData[i];
                     var t = (pt.value[0] instanceof Date) ? pt.value[0].getTime() : pt.value[0];
                     if (t < startTime) {
                         lastValues[c.id] = pt.value[1];
                         break; 
                     }
                 }
             }
        });

        // Build CSV content
        var csv = headers.join(",") + "\n";
        
        // Sort timestamps and generate rows
        var sortedTimestamps = Array.from(dataMap.keys()).sort((a,b) => a - b);
        
        sortedTimestamps.forEach(ts => {
            var row = dataMap.get(ts);
            
            // Update lastValues with current row's data
            keys.forEach(key => {
                if (row[key] !== undefined && row[key] !== null) {
                    lastValues[key] = row[key];
                }
            });
            
            var dateStr = new Date(ts).toISOString();
            var line = [dateStr];
            
            keys.forEach(key => {
                var val = lastValues[key]; 
                line.push((val !== undefined && val !== null) ? val : "");
            });
            csv += line.join(",") + "\n";
        });
        
        var blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });

        // Try to save to FileSystem Handle first
        if (window.saveBlobToLogDir) {
            window.saveBlobToLogDir(filename, blob).then(success => {
                if (success) {
                   if (isAuto) console.log("Auto-captured graph data to disk: " + filename);
                } else {
                   this._downloadBlob(blob, filename);
                }
            });
        } else {
             this._downloadBlob(blob, filename);
        }
    },

    screenshot: function(isAuto) {
        if (!this.chart) return;
        
        var url = this.chart.getDataURL({
            type: 'png',
            pixelRatio: 2,
            backgroundColor: this.currentTheme === 'dark' ? '#1e1e1e' : '#fff'
        });
        
        var now = new Date();
        var iso = now.toISOString().replace(/[:.]/g, '-').slice(0, -5); // format: YYYY-MM-DDTHH-mm-ss
        var prefix = isAuto ? 'otgw-graph-auto-' : 'otgw-graph-';
        var filename = prefix + iso + '.png';

        // Try to safe to FileSystem Handle first (if available via index.js helper)
        if (window.saveBlobToLogDir) {
            // Convert DataURL to Blob
            var arr = url.split(',');
            var mimeMatch = arr[0].match(/:(.*?);/);
            if (!mimeMatch || !mimeMatch[1]) {
                console.error('Failed to extract MIME type from data URL');
                return;
            }
            var mime = mimeMatch[1];
            var bstr = atob(arr[1]), n = bstr.length, u8arr = new Uint8Array(n);
            while(n--) u8arr[n] = bstr.charCodeAt(n);
            var blob = new Blob([u8arr], {type:mime});

            window.saveBlobToLogDir(filename, blob).then(success => {
                if (success) {
                   if (isAuto) console.log("Auto-captured graph screenshot to disk: " + filename);
                } else {
                   // Fallback to download if save failed (e.g. handle not set)
                   this._downloadFile(url, filename);
                }
            });
        } else {
             this._downloadFile(url, filename);
        }
    },

    _downloadFile: function(url, filename) {
        var link = document.createElement('a');
        link.download = filename;
        link.href = url;
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
    },

    _downloadBlob: function(blob, filename) {
        var link = document.createElement('a');
        if (link.download !== undefined) { 
            var url = URL.createObjectURL(blob);
            link.setAttribute("href", url);
            link.setAttribute("download", filename);
            link.style.visibility = 'hidden';
            document.body.appendChild(link);
            link.click();
            document.body.removeChild(link);
        }
    },

    setTheme: function(newTheme) {
        if (this.currentTheme === newTheme) return;
        this.currentTheme = newTheme;
        
        if (this.chart) {
            // Preserve old chart instance locally and clear reference to prevent
            // keeping a disposed-but-non-null chart in this.chart
            var oldChart = this.chart;
            this.chart = null;

            try {
                // Dispose the previous chart instance
                oldChart.dispose();
            } catch (e) {
                console.error('Error disposing existing chart:', e);
            }

            var container = document.getElementById('otGraphCanvas');
            if (!container) {
                console.error('Graph container not found');
                return;
            }

            try {
                var newChart = echarts.init(container, newTheme);
                this.chart = newChart;
                this.updateOption();
                this.resize();
            } catch (e) {
                // On any error during re-initialization, ensure chart reference
                // remains in a consistent state (null)
                this.chart = null;
                console.error('Error changing theme:', e);
            }
        }
    },

    updateOption: function() {
        if (!this.chart) return;
        
        var palette = this.palettes[this.currentTheme] || this.palettes.light;

        var option = {
            tooltip: {
                trigger: 'axis',
                axisPointer: { type: 'cross' }
            },
            // Panel titles for each sub-graph
            title: [
                { text: 'Flame Status', left: '1%', top: '6%', textStyle: { fontSize: 12, fontWeight: 'bold' } },
                { text: 'DHW Mode', left: '1%', top: '16%', textStyle: { fontSize: 12, fontWeight: 'bold' } },
                { text: 'CH Mode', left: '1%', top: '26%', textStyle: { fontSize: 12, fontWeight: 'bold' } },
                { text: 'Modulation', left: '1%', top: '39%', textStyle: { fontSize: 12, fontWeight: 'bold' } },
                { text: 'Temperatures', left: '1%', top: '64%', textStyle: { fontSize: 12, fontWeight: 'bold' } }
            ],
            // Legend specifically for temperature panel (shows all temp series with colors)
            // Dynamically build legend data to include Dallas sensors
            legend: {
                data: this.seriesConfig
                    .filter(function(c) { return c.gridIndex === 4; }) // Only temperature grid series
                    .map(function(c) { return c.label; }),
                top: '62%',
                left: '15%',
                orient: 'horizontal',
                type: 'scroll',
                textStyle: { fontSize: 11 }
            },
            grid: [
                // 5 vertical grids - adjusted left margin to accommodate Y-axis labels
                // 0: Flame (Top)
                { left: '12%', right: '5%', top: '5%', height: '8%', containLabel: true }, 
                // 1: DHW Mode
                { left: '12%', right: '5%', top: '15%', height: '8%', containLabel: true }, 
                // 2: CH Mode
                { left: '12%', right: '5%', top: '25%', height: '8%', containLabel: true }, 
                // 3: Modulation
                { left: '12%', right: '5%', top: '38%', height: '20%', containLabel: true }, 
                // 4: Temps (Bottom section, largest)
                { left: '12%', right: '5%', top: '67%', bottom: '5%', containLabel: true } 
            ],
            axisPointer: {
                link: { xAxisIndex: 'all' }
            },
            xAxis: [
                // One x-axis per grid, linked visually
                { type: 'time', gridIndex: 0, axisLabel: { show: false }, splitLine: { show: true } },
                { type: 'time', gridIndex: 1, axisLabel: { show: false }, splitLine: { show: true } },
                { type: 'time', gridIndex: 2, axisLabel: { show: false }, splitLine: { show: true } },
                { type: 'time', gridIndex: 3, axisLabel: { show: false }, splitLine: { show: true } },
                { type: 'time', gridIndex: 4, axisLabel: { show: true },  splitLine: { show: true }, min: this.getMinTime() }
            ],
            yAxis: [
                // 0: Flame (0-1) with On/Off labels
                { 
                    type: 'value', 
                    gridIndex: 0, 
                    min: 0, 
                    max: 1.2, 
                    interval: 1, 
                    splitLine: { show: false }, 
                    axisLabel: { 
                        show: true,
                        formatter: function(value) {
                            if (value === 0) return 'Off';
                            if (value === 1) return 'On';
                            return '';
                        }
                    }
                },
                // 1: DHW (0-1) with On/Off labels
                { 
                    type: 'value', 
                    gridIndex: 1, 
                    min: 0, 
                    max: 1.2, 
                    interval: 1, 
                    splitLine: { show: false }, 
                    axisLabel: { 
                        show: true,
                        formatter: function(value) {
                            if (value === 0) return 'Off';
                            if (value === 1) return 'On';
                            return '';
                        }
                    }
                },
                // 2: CH (0-1) with On/Off labels
                { 
                    type: 'value', 
                    gridIndex: 2, 
                    min: 0, 
                    max: 1.2, 
                    interval: 1, 
                    splitLine: { show: false }, 
                    axisLabel: { 
                        show: true,
                        formatter: function(value) {
                            if (value === 0) return 'Off';
                            if (value === 1) return 'On';
                            return '';
                        }
                    }
                },
                // 3: Mod (0-100) with percentage labels
                { 
                    type: 'value', 
                    gridIndex: 3, 
                    min: 0, 
                    max: 100, 
                    splitLine: { show: true },
                    axisLabel: { 
                        show: true,
                        formatter: '{value}%'
                    }
                },
                // 4: Temps with degree Celsius labels
                { 
                    type: 'value', 
                    gridIndex: 4, 
                    splitLine: { show: true },
                    axisLabel: { 
                        show: true,
                        formatter: '{value}°C'
                    }
                }
            ],
            series: this.seriesConfig.map((c, idx) => {
                var seriesConfig = {
                    name: c.label,
                    type: c.type,
                    step: c.step,
                    xAxisIndex: c.gridIndex,
                    yAxisIndex: c.gridIndex,
                    showSymbol: false,
                    lineStyle: { width: 1.5 }, // Slightly thinner lines for better performance with dense data
                    itemStyle: { color: palette[c.id] }, // Get color from palette
                    areaStyle: c.areaStyle || undefined,
                    large: c.large,        // Enable large dataset optimization
                    sampling: c.sampling,  // Enable downsampling
                    data: this.data[c.id]
                };
                
                // Add disconnect/reconnect markers to the first series of each grid to avoid duplicates
                // Only add to first series in each grid: flame(0), dhwMode(1), chMode(2), mod(3), ctrlSp(4)
                if (idx === 0 || idx === 1 || idx === 2 || idx === 3 || idx === 4) {
                    var markLineData = [];
                    this.disconnectMarkers.forEach(function(marker) {
                        var isDisconnect = marker.type === 'disconnect';
                        markLineData.push({
                            xAxis: marker.time,
                            lineStyle: {
                                color: isDisconnect ? '#ff4444' : '#44ff44',
                                type: 'dashed',
                                width: 2
                            },
                            label: {
                                show: false
                            }
                        });
                    });
                    
                    if (markLineData.length > 0) {
                        seriesConfig.markLine = {
                            silent: true,
                            symbol: 'none',
                            data: markLineData
                        };
                    }
                }
                
                return seriesConfig;
            })
        };

        this.chart.setOption(option);
    },


    resize: function() {
        if (this.chart) {
            this.chart.resize();
        }
    },

    recordDisconnect: function() {
        var now = new Date().getTime();
        this.disconnectMarkers.push({ time: now, type: 'disconnect' });
        console.log('Graph: Disconnect marker added at', new Date(now).toISOString());
        // Trim old markers outside current max time window (24h)
        var cutoff = now - (24 * 3600 * 1000);
        this.disconnectMarkers = this.disconnectMarkers.filter(function(m) { return m.time > cutoff; });
        
        // Update chart to display the marker immediately
        if (this.chart) {
            this.updateOption();
        }
    },

    recordReconnect: function() {
        var now = new Date().getTime();
        this.disconnectMarkers.push({ time: now, type: 'reconnect' });
        console.log('Graph: Connected marker added at', new Date(now).toISOString());
        // Trim old markers outside current max time window (24h)
        var cutoff = now - (24 * 3600 * 1000);
        this.disconnectMarkers = this.disconnectMarkers.filter(function(m) { return m.time > cutoff; });
        
        // Update chart to display the marker immediately
        if (this.chart) {
            this.updateOption();
        }
    },

    processLine: function(line) {
        if (!this.running || !line || typeof line !== 'object') return; 
        
        // Only process valid data (valid field should be '>' for valid messages)
        if (!line.valid || line.valid !== '>') return;
        
        try {
            var id = parseInt(line.id, 10);
            var now = new Date(); 
            
            if (isNaN(id)) return;

            if (id === 0) {
                      // Status bits: derive Flame/DHW/CH from the slave status flags using F/W/C symbols.
                      // Prefer structured JSON: line.data.slave is an 8-char flag string like "-CWF-2-D".
                      // (Fallback to extracting from line.value only if data.slave is missing.)
                      var slaveFlags = (line.data && typeof line.data === 'object') ? line.data.slave : null;
                      if ((typeof slaveFlags !== 'string' || slaveFlags.length === 0) && typeof line.value === 'string') {
                          var m = /Slave\s*\[([^\]]+)\]/.exec(line.value);
                          if (m && m[1]) slaveFlags = m[1];
                      }

                      if (typeof slaveFlags === 'string' && slaveFlags.length >= 4) {
                          // Keep the original bit positions used by the firmware flag string:
                          // index 1 = CH mode ('C'), index 2 = DHW mode ('W'), index 3 = Flame ('F')
                          var ch    = (slaveFlags.charAt(1) === 'C') ? 1 : 0;
                          var dhw   = (slaveFlags.charAt(2) === 'W') ? 1 : 0;
                          var flame = (slaveFlags.charAt(3) === 'F') ? 1 : 0;
                          this.pushData('flame', now, flame);
                          this.pushData('dhwMode', now, dhw);
                          this.pushData('chMode', now, ch);
                      }
            } else {
                 // Analog values: use numeric JSON field `val` only.
                 // Fallback to parsing numeric value from string `value` if `val` is missing (legacy log lines)
                 var val = null;
                 if (line.val !== undefined && line.val !== null && typeof line.val === 'number') {
                     val = line.val;
                 } else if (typeof line.value === 'string') {
                     // Try to parse number from start of string (e.g. "35.70 °C")
                     var parsed = parseFloat(line.value);
                     if (!isNaN(parsed)) {
                         val = parsed;
                     }
                 }

                 if (val === null || !isFinite(val)) return;
                 
                 var key = null;
                 switch(id) {
                     case 17: 
                         key = 'mod';
                         // Modulation should be 0-100%
                         if (val < 0 || val > 100) return;
                         break;
                     case 1:  
                         key = 'ctrlSp';
                         // Temperature bounds check (reasonable range -50°C to 150°C)
                         if (val < -50 || val > 150) return;
                         break;
                     case 25: 
                         key = 'boiler';
                         if (val < -50 || val > 150) return;
                         break;
                     case 28: 
                         key = 'return';
                         if (val < -50 || val > 150) return;
                         break;
                     case 16: 
                         key = 'roomSp';
                         if (val < -50 || val > 150) return;
                         break;
                     case 24: 
                         key = 'room';
                         if (val < -50 || val > 150) return;
                         break;
                     case 27: 
                         key = 'outside';
                         if (val < -50 || val > 150) return;
                         break;
                 }
                 if (key) this.pushData(key, now, val);
            }
        } catch(e) {
            console.error("Error processing line", e);
        }
    },

    pushData: function(key, time, value) {
        if (!this.data[key]) return;
        
        var dataPoint = {
            name: time.toString(),
            value: [time, value]
        };
        
        this.data[key].push(dataPoint);
        
        // Track this point as pending for incremental chart update
        if (this.pendingData[key]) {
            this.pendingData[key].push(dataPoint);
        }
        
        // Trim old data if exceeds maxPoints
        if (this.data[key].length > this.maxPoints) {
            this.data[key].shift();
            // Note: pendingData only contains recent points (cleared every 2s)
            // so there's no need to sync the shift operation
        }
    },

    getMinTime: function() {
        return (new Date()).getTime() - this.timeWindow;
    },

    updateChart: function() {
        if (!this.chart) return;
        
        // Update xAxis min to ensure scrolling window logic
        var minTime = this.getMinTime();

        // Build array of xAxis updates for the sliding window
        var xAxisUpdate = [];
        for(var i=0; i<5; i++) {
            xAxisUpdate.push({ min: minTime });
        }

        // Check if there's any pending data to append
        var hasPendingData = false;
        for (var key in this.pendingData) {
            if (this.pendingData[key] && this.pendingData[key].length > 0) {
                hasPendingData = true;
                break;
            }
        }

        if (hasPendingData) {
            // Use incremental update for better performance
            // Process each series and append new data points
            this.seriesConfig.forEach(function(c, index) {
                var pending = this.pendingData[c.id];
                if (pending && pending.length > 0) {
                    // ECharts appendData expects an array of values
                    var values = pending.map(function(p) { return p.value; });
                    this.chart.appendData({
                        seriesIndex: index,
                        data: values
                    });
                }
            }.bind(this));

            // Clear pending data after appending
            this.seriesConfig.forEach(function(c) {
                this.pendingData[c.id] = [];
            }.bind(this));
        }

        // Always update xAxis for the sliding window
        this.chart.setOption({
            xAxis: xAxisUpdate
        });
    },
    
    dispose: function() {
        console.log("OTGraph dispose");
        
        // Stop running
        this.running = false;
        
        // Clear timers
        if (this.updateTimer) {
            clearInterval(this.updateTimer);
            this.updateTimer = null;
        }
        if (this.captureTimer) {
            clearInterval(this.captureTimer);
            this.captureTimer = null;
        }
        if (this.exportTimer) {
            clearInterval(this.exportTimer);
            this.exportTimer = null;
        }
        
        // Remove resize handler
        if (this.resizeHandler) {
            window.removeEventListener('resize', this.resizeHandler);
            this.resizeHandler = null;
        }
        
        // Remove DOM event listeners
        var timeSelect = document.getElementById('graphTimeWindow');
        if (timeSelect && this.timeWindowHandler) {
            timeSelect.removeEventListener('change', this.timeWindowHandler);
            this.timeWindowHandler = null;
        }
        
        var btnShot = document.getElementById('btnGraphScreenshot');
        if (btnShot && this.screenshotHandler) {
            btnShot.removeEventListener('click', this.screenshotHandler);
            this.screenshotHandler = null;
        }
        
        var chkAutoShot = document.getElementById('chkAutoScreenshot');
        if (chkAutoShot && this.autoScreenshotHandler) {
            chkAutoShot.removeEventListener('change', this.autoScreenshotHandler);
            this.autoScreenshotHandler = null;
        }
        
        var btnExport = document.getElementById('btnGraphExport');
        if (btnExport && this.exportHandler) {
            btnExport.removeEventListener('click', this.exportHandler);
            this.exportHandler = null;
        }
        
        var chkAutoExport = document.getElementById('chkAutoExport');
        if (chkAutoExport && this.autoExportHandler) {
            chkAutoExport.removeEventListener('change', this.autoExportHandler);
            this.autoExportHandler = null;
        }
        
        // Dispose chart
        if (this.chart) {
            try {
                this.chart.dispose();
            } catch(e) {
                console.warn('Error disposing chart:', e);
            }
            this.chart = null;
        }
        
        this.initialized = false;
    }
};

/*
MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
