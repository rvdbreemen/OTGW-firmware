/*
***************************************************************************  
**  Program  : graph.js, part of OTGW-firmware project
**  Version  : v2.0.0 (ECharts version)
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

var OTGraph = {
    chart: null,
    data: {},
    maxPoints: 432000, // Buffer to hold ~24h of data at 5 msgs/sec
    timeWindow: 3600 * 1000, // Default 1 Hour in milliseconds
    running: false,
    updateTimer: null,
    currentTheme: 'light',
    lastUpdate: 0,
    updateInterval: 2000, // Update every 2 seconds to reduce load

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
        { id: 'flame',   label: 'Flame',    gridIndex: 0, type: 'line', step: 'start', areaStyle: { opacity: 0.3 } },
        { id: 'dhwMode', label: 'DHW Mode', gridIndex: 1, type: 'line', step: 'start', areaStyle: { opacity: 0.3 } },
        { id: 'chMode',  label: 'CH Mode',  gridIndex: 2, type: 'line', step: 'start', areaStyle: { opacity: 0.3 } },
        { id: 'mod',     label: 'Modulation (%)',   gridIndex: 3, type: 'line', step: false },
        { id: 'ctrlSp',  label: 'Control SP',       gridIndex: 4, type: 'line', step: false },
        { id: 'boiler',  label: 'Boiler Temp',      gridIndex: 4, type: 'line', step: false },
        { id: 'return',  label: 'Return Temp',      gridIndex: 4, type: 'line', step: false },
        { id: 'roomSp',  label: 'Room SP',          gridIndex: 4, type: 'line', step: false },
        { id: 'room',    label: 'Room Temp',        gridIndex: 4, type: 'line', step: false },
        { id: 'outside', label: 'Outside Temp',     gridIndex: 4, type: 'line', step: false }
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
            container.innerHTML = "Error: ECharts library not loaded. Internet connection required for CDN.";
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

        // Initialize empty data arrays if not present
        this.seriesConfig.forEach(c => {
            if (!this.data[c.id]) this.data[c.id] = [];
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

    cleanup: function() {
        // Clear the update timer to prevent memory leaks
        if (this.updateTimer) {
            clearInterval(this.updateTimer);
            this.updateTimer = null;
        }
        this.running = false;
    },

    destroy: function() {
        // Full cleanup: stop updates and dispose chart
        this.cleanup();
        if (this.chart) {
            this.chart.dispose();
            this.chart = null;
        }
    },

    setTheme: function(newTheme) {
        if (this.currentTheme === newTheme) return;
        this.currentTheme = newTheme;
        
        if (this.chart) {
            // Clean up timer before disposing chart
            this.cleanup();
            this.chart.dispose();
            var container = document.getElementById('otGraphCanvas');
            this.chart = echarts.init(container, newTheme);
            this.updateOption();
            this.resize();
            // Restart the update timer after re-initialization
            this.running = true;
            this.updateTimer = setInterval(() => {
                requestAnimationFrame(() => this.updateChart());
            }, this.updateInterval);
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
                // Margins: left 50px for axes labels. Right 20px.
                // 0: Flame (Top)
                { left: '50', right: '20', top: '5%', height: '8%', containLabel: false }, 
                // 1: DHW Mode
                { left: '50', right: '20', top: '15%', height: '8%', containLabel: false }, 
                // 2: CH Mode
                { left: '50', right: '20', top: '25%', height: '8%', containLabel: false }, 
                // 3: Modulation
                { left: '50', right: '20', top: '38%', height: '20%', containLabel: false }, 
                // 4: Temps (Bottom section, largest)
                { left: '50', right: '20', top: '63%', bottom: '5%', containLabel: false } 
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
            series: this.seriesConfig.map(c => ({
                name: c.label,
                type: c.type,
                step: c.step,
                xAxisIndex: c.gridIndex,
                yAxisIndex: c.gridIndex,
                showSymbol: false,
                lineStyle: { width: 2 },
                itemStyle: { color: palette[c.id] }, // Get color from palette
                areaStyle: c.areaStyle || undefined,
                data: this.data[c.id]
            }))
        };

        this.chart.setOption(option);
    },


    resize: function() {
        if (this.chart) {
            this.chart.resize();
        }
    },

    processLine: function(line) {
        if (!this.running || !line || line.length < 5) return; // Need at least ID and some content
        // Basic check for log line validity
        if (line.indexOf('>') === -1) return;

        try {
            // Firmware log format: "  ID Type       >Label = Value"
            // Example with timestamp: "12:34:56.789   0 Read-Ack        >Status = Slave  [E-C-W-F...]"
            // Or raw: "   0 Read-Ack        >Status = Slave  [E-C-W-F...]"
            
            // Regex to find ID: look for (digits) (space) (Type) (space) (>)
            // or simply match the ID before the Type column.
            // The type seems to be stuck to > for some messages or spaced out.
            // Let's use a more flexible regex that looks for the decimal ID 
            // optionally preceded by timestamp/spaces, and followed by text then >
            
            // Strategy: Look for the segment "  ID " or " ID " before the ">"
            // The firmware prints " %3d", so "  0", " 10", "100".
            
            var id = NaN;
            
            // Try matching ID at start (raw firmware output)
            var matchStart = /^\s*(\d+)\s/.exec(line);
            if (matchStart) {
                id = parseInt(matchStart[1], 10);
            } else {
                 // Try finding ID inside the string (if timestamp is present)
                 // Look for 1-3 digits followed by a known message type or just spaces and text then >
                 // Example: "... 123456   0 Read-Ack"
                 // Let's rely on the > separator.
                 // The ID is usually the first number on the line if we split by space? 
                 // No, timestamp has numbers.
                 
                 // Firmware specific format: "   0 Read-Ack"
                 // It matches: space(s) digits space(s) Word
                 var matchInside = /\s+(\d+)\s+[a-zA-Z\-]+\s*>/;
                 var m = line.match(matchInside);
                 if (m) {
                     id = parseInt(m[1], 10);
                 }
            }
            
            if (isNaN(id)) return;
            
            var now = new Date();
            var val = 0;
            
            if (id === 0) {
                // Status MsgID 0 - Slave Information
                // The binary information should be derived from the id 0 from the slave information.
                // Encoding from firmware (OTGW-Core.ino):
                //  0: Fault 'E'
                //  1: CH mode 'C'
                //  2: DHW mode 'W'
                //  3: Flame status 'F'
                //  4: Cooling status 'C'
                //  5: CH2 mode '2'
                //  6: Diagnostic 'D'
                //  7: Electric 'P'
                
                var match = /Slave\s*\[([A-Z\-\.]{8})\]/.exec(line);
                if (match) {
                    var chars = match[1]; 
                    // Map characters to status (1 if char matches active code, 0 otherwise)
                    var ch    = (chars.charAt(1) === 'C') ? 1 : 0;
                    var dhw   = (chars.charAt(2) === 'W') ? 1 : 0;
                    var flame = (chars.charAt(3) === 'F') ? 1 : 0;
                    
                    this.pushData('flame', now, flame);
                    this.pushData('dhwMode', now, dhw);
                    this.pushData('chMode', now, ch);
                } else {
                    // Fallback for potential legacy binary format: LB flag8[xxxxxxxx]
                    var binMatch = /LB flag8\[([01]{8})\]/.exec(line);
                    if (binMatch) {
                        var bits = binMatch[1]; 
                        var flame = bits.charAt(4) === '1' ? 1 : 0;
                        var dhw   = bits.charAt(5) === '1' ? 1 : 0;
                        var ch    = bits.charAt(6) === '1' ? 1 : 0;
                        
                        this.pushData('flame', now, flame);
                        this.pushData('dhwMode', now, dhw);
                        this.pushData('chMode', now, ch);
                    }
                }
            } else {
                 var parts = line.split('=');
                 if (parts.length < 2) return;
                 var valPart = parts[parts.length-1].trim(); 
                 val = parseFloat(valPart);
                 if (isNaN(val)) return;
    
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
        
        this.data[key].push({
            name: time.toString(),
            value: [time, value]
        });
        
        if (this.data[key].length > this.maxPoints) {
            this.data[key].shift();
        }
    },

    getMinTime: function() {
        return (new Date()).getTime() - this.timeWindow;
    },

    updateChart: function() {
        if (!this.chart) return;
        
        // Update xAxis min to ensure scrolling window logic (1 hour scope)
        var minTime = this.getMinTime();

        var seriesUpdate = this.seriesConfig.map(c => ({
            name: c.label,
            data: this.data[c.id]
        }));
        
        // Update all x-axes to have the correct min value for the sliding window
        // We have 5 x-axes now
        var xAxisUpdate = [];
        for(var i=0; i<5; i++) {
            xAxisUpdate.push({ min: minTime });
        }

        this.chart.setOption({
            xAxis: xAxisUpdate,
            series: seriesUpdate
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
