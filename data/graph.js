/*
***************************************************************************  
**  Program  : graph.js, part of OTGW-firmware project
**  Version  : v1.0.0-rc6
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

// Configuration constants
const UPDATE_INTERVAL_MS = 2000; // Update chart every 2 seconds to reduce load

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

    // 5 separate grids/graphs
    // 0: Flame (Digital)
    // 1: DHW Mode (Digital)
    // 2: CH Mode (Digital)
    // 3: Modulation (Analog 0-100)
    // 4: Temperature (Analog)
    seriesConfig: [
        { id: 'flame',   label: 'Flame',    gridIndex: 0, type: 'line', step: 'start', areaStyle: { opacity: 0.3 }, large: true, sampling: 'lttb' },
        { id: 'dhwMode', label: 'DHW Mode', gridIndex: 1, type: 'line', step: 'start', areaStyle: { opacity: 0.3 }, large: true, sampling: 'lttb' },
        { id: 'chMode',  label: 'CH Mode',  gridIndex: 2, type: 'line', step: 'start', areaStyle: { opacity: 0.3 }, large: true, sampling: 'lttb' },
        { id: 'mod',     label: 'Modulation (%)',   gridIndex: 3, type: 'line', step: false, large: true, sampling: 'lttb' },
        { id: 'ctrlSp',  label: 'Control SP',       gridIndex: 4, type: 'line', step: false, large: true, sampling: 'lttb' },
        { id: 'boiler',  label: 'Boiler Temp',      gridIndex: 4, type: 'line', step: false, large: true, sampling: 'lttb' },
        { id: 'return',  label: 'Return Temp',      gridIndex: 4, type: 'line', step: false, large: true, sampling: 'lttb' },
        { id: 'roomSp',  label: 'Room SP',          gridIndex: 4, type: 'line', step: false, large: true, sampling: 'lttb' },
        { id: 'room',    label: 'Room Temp',        gridIndex: 4, type: 'line', step: false, large: true, sampling: 'lttb' },
        { id: 'outside', label: 'Outside Temp',     gridIndex: 4, type: 'line', step: false, large: true, sampling: 'lttb' }
    ],

    init: function() {
        console.log("OTGraph init (ECharts)");
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
        
        // Bind settings
        var timeSelect = document.getElementById('graphTimeWindow');
        if (timeSelect) {
            timeSelect.addEventListener('change', (e) => {
                this.setTimeWindow(parseInt(e.target.value, 10));
            });
            // Set initial value directly to avoid calling updateChart before init
            var initialMinutes = parseInt(timeSelect.value, 10);
            if (initialMinutes && !isNaN(initialMinutes)) {
                this.timeWindow = initialMinutes * 60 * 1000;
            }
        }
        
        var btnShot = document.getElementById('btnGraphScreenshot');
        if (btnShot) {
            btnShot.addEventListener('click', () => {
                this.screenshot(false);
            });
        }
        
        var chkAutoShot = document.getElementById('chkAutoScreenshot');
        if (chkAutoShot) {
            chkAutoShot.addEventListener('change', (e) => {
                 this.toggleAutoScreenshot(e.target.checked);
                 if (typeof saveUISetting === 'function') saveUISetting('ui_autoscreenshot', e.target.checked);
            });
        }
        
        var btnExport = document.getElementById('btnGraphExport');
        if (btnExport) {
            btnExport.addEventListener('click', () => {
                this.exportData(false);
            });
        }

        var chkAutoExport = document.getElementById('chkAutoExport');
        if (chkAutoExport) {
            chkAutoExport.addEventListener('change', (e) => {
                 this.toggleAutoExport(e.target.checked);
                 if (typeof saveUISetting === 'function') saveUISetting('ui_autoexport', e.target.checked);
            });
        }

        // Initialize empty data arrays if not present
        this.seriesConfig.forEach(c => {
            if (!this.data[c.id]) this.data[c.id] = [];
            if (!this.pendingData[c.id]) this.pendingData[c.id] = [];
        });

        this.updateOption();
        
        this.running = true;
        
        // Handle resize
        window.addEventListener('resize', () => {
            if (this.chart) this.chart.resize();
        });

        // Throttle updates to chart: use requestAnimationFrame with throttling
        if (this.updateTimer) clearInterval(this.updateTimer);
        this.updateTimer = setInterval(() => {
            requestAnimationFrame(() => this.updateChart());
        }, this.updateInterval);
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

        // Header
        var csv = "Timestamp,Dataset,Value\n";
        
        // Collect data in current time window
        var startTime = now.getTime() - this.timeWindow;
        
        // Iterate all active series
        this.seriesConfig.forEach(c => {
             if (this.data[c.id]) {
                 this.data[c.id].forEach(pt => {
                     // pt is {name: time, value: [timestamp, value]}
                     if (pt.value[0] >= startTime) {
                         // Format timestamp to ISO
                         var ts = new Date(pt.value[0]).toISOString();
                         csv += `${ts},${c.label},${pt.value[1]}\n`;
                     }
                 });
             }
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
            var arr = url.split(','), mime = arr[0].match(/:(.*?);/)[1],
                bstr = atob(arr[1]), n = bstr.length, u8arr = new Uint8Array(n);
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
            this.chart.dispose();
            var container = document.getElementById('otGraphCanvas');
            this.chart = echarts.init(container, newTheme);
            this.updateOption();
            this.resize();
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
            legend: {
                data: this.seriesConfig.map(c => c.label),
                top: 0,
                type: 'scroll'
            },
            grid: [
                // 5 vertical grids
                // Margins: use percentages for responsive layout (space for axes labels on the left).
                // 0: Flame (Top)
                { left: '10%', right: '5%', top: '5%', height: '8%', containLabel: false }, 
                // 1: DHW Mode
                { left: '10%', right: '5%', top: '15%', height: '8%', containLabel: false }, 
                // 2: CH Mode
                { left: '10%', right: '5%', top: '25%', height: '8%', containLabel: false }, 
                // 3: Modulation
                { left: '10%', right: '5%', top: '38%', height: '20%', containLabel: false }, 
                // 4: Temps (Bottom section, largest)
                { left: '10%', right: '5%', top: '63%', bottom: '5%', containLabel: false } 
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
                // 0: Flame (0-1)
                { type: 'value', gridIndex: 0, min: 0, max: 1.2, interval: 1, splitLine: { show: false }, axisLabel: { show: false } },
                // 1: DHW (0-1)
                { type: 'value', gridIndex: 1, min: 0, max: 1.2, interval: 1, splitLine: { show: false }, axisLabel: { show: false } },
                // 2: CH (0-1)
                { type: 'value', gridIndex: 2, min: 0, max: 1.2, interval: 1, splitLine: { show: false }, axisLabel: { show: false } },
                // 3: Mod (0-100)
                { type: 'value', gridIndex: 3, min: 0, max: 100, splitLine: { show: true } },
                // 4: Temps
                { type: 'value', gridIndex: 4, splitLine: { show: true } }
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
                                show: true,
                                position: 'end',
                                formatter: isDisconnect ? 'Disconnected' : 'Reconnected',
                                color: isDisconnect ? '#ff4444' : '#44ff44',
                                fontSize: 10
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
    },

    recordReconnect: function() {
        var now = new Date().getTime();
        this.disconnectMarkers.push({ time: now, type: 'reconnect' });
        console.log('Graph: Reconnect marker added at', new Date(now).toISOString());
        // Trim old markers outside current max time window (24h)
        var cutoff = now - (24 * 3600 * 1000);
        this.disconnectMarkers = this.disconnectMarkers.filter(function(m) { return m.time > cutoff; });
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

                 if (val === null) return;
                 
                 var key = null;
                 switch(id) {
                     case 17: key = 'mod'; break;
                     case 1:  key = 'ctrlSp'; break;
                     case 25: key = 'boiler'; break;
                     case 28: key = 'return'; break;
                     case 16: key = 'roomSp'; break;
                     case 24: key = 'room'; break;
                     case 27: key = 'outside'; break;
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
