/*
***************************************************************************  
**  Program  : graph.js, part of OTGW-firmware project
**  Version  : v1.0.0
**
**  Copyright (c) 2021-2024 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

var OTGraph = {
    canvas: null,
    ctx: null,
    data: {},
    maxPoints: 3000, 
    running: false,

    // Colors matching user request
    config: [
        { id: 'dhwMode', color: 'blue',    type: 'digital', label: 'DHW Mode' },
        { id: 'chMode',  color: 'green',   type: 'digital', label: 'CH Mode' },
        { id: 'flame',   color: 'red',     type: 'digital', label: 'Flame' },
        { id: 'mod',     color: 'black',   type: 'analog',  label: 'Modulation' },
        { id: 'ctrlSp',  color: 'grey',    type: 'analog',  label: 'Control SP' },
        { id: 'boiler',  color: 'red',     type: 'analog',  label: 'Boiler Temp' },
        { id: 'return',  color: 'blue',    type: 'analog',  label: 'Return Temp' },
        { id: 'roomSp',  color: 'cyan',    type: 'analog',  label: 'Room SP' },
        { id: 'room',    color: 'magenta', type: 'analog',  label: 'Room Temp' },
        { id: 'outside', color: 'green',   type: 'analog',  label: 'Outside Temp' }
    ],

    init: function() {
        console.log("OTGraph init");
        this.canvas = document.getElementById('otGraphCanvas');
        if (!this.canvas) return;
        this.ctx = this.canvas.getContext('2d');
        
        // Init buffers
        this.config.forEach(c => this.data[c.id] = []);
        
        this.resize();
        window.addEventListener('resize', () => this.resize());
        
        this.running = true;
        requestAnimationFrame(() => this.loop());
    },

    resize: function() {
        if (!this.canvas) return;
        var parent = this.canvas.parentElement;
        if (parent) {
            this.canvas.width = parent.clientWidth - 20; // margin
            this.canvas.height = 400; 
        }
    },

    processLine: function(line) {
        if (!line || line.length < 40 || line.charAt(39) !== '>') return;
        
        var idStr = line.substring(18, 22).trim();
        var id = parseInt(idStr, 10);
        if (isNaN(id)) return;
        
        var now = Date.now();
        var val = 0;
        
        if (id === 0) {
            // Status. Need to find "LB flag8[xxxxxxxx]"
            var match = /LB flag8\[([01]{8})\]/.exec(line);
            if (match) {
                var bits = match[1]; 
                // "00000000": index 0 is MSB (bit 7)
                // Bit 3 (Flame) is index 4.
                // Bit 2 (DHW) is index 5.
                // Bit 1 (CH) is index 6.
                
                var flame = bits.charAt(4) === '1' ? 1 : 0;
                var dhw   = bits.charAt(5) === '1' ? 1 : 0;
                var ch    = bits.charAt(6) === '1' ? 1 : 0;
                
                this.pushData('flame', now, flame);
                this.pushData('dhwMode', now, dhw);
                this.pushData('chMode', now, ch);
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
    },

    pushData: function(key, t, v) {
        this.data[key].push({t: t, v: v});
        while(this.data[key].length > this.maxPoints) this.data[key].shift();
    },

    loop: function() {
        if (this.running) {
             var tab = document.getElementById('Graph');
             if (tab && tab.classList.contains('active')) {
                 this.draw();
             }
             requestAnimationFrame(() => this.loop());
        }
    },

    draw: function() {
        var w = this.canvas.width;
        var h = this.canvas.height;
        var ctx = this.ctx;
        
        ctx.clearRect(0, 0, w, h);
        
        // Define areas: Top 15% for Digital, Bottom 85% for Analog
        var hDig = h * 0.15;
        var hAna = h - hDig;
        var yAnaStart = hDig;
        
        var now = Date.now();
        var timeWindow = 600 * 1000; // 10 minutes default sliding window
        var tStart = now - timeWindow;
        
        // Clear background
        ctx.fillStyle = '#ffffff';
        ctx.fillRect(0,0,w,h);

        // Draw Reference Grid for Analog
        ctx.strokeStyle = '#eee';
        ctx.lineWidth = 1;
        ctx.beginPath();
        for(var i=0; i<=10; i++) {
            var y = yAnaStart + hAna - (i/10)*hAna;
            ctx.moveTo(0, y);
            ctx.lineTo(w, y);
        }
        ctx.stroke();

        // Draw Legend / Labels text
        ctx.font = "10px Arial";

        // Digital channels
        var digChs = ['dhwMode', 'chMode', 'flame'];
        var slice = hDig / 3;
        
        digChs.forEach((key, idx) => {
            var buf = this.data[key];
            var conf = this.config.find(c => c.id === key);
            ctx.fillStyle = conf.color;
            ctx.strokeStyle = conf.color; 
            
            var yBase = slice * (idx + 1) - 2; 
            var yHigh = slice * idx + 2;      
            
            // Label
            ctx.fillText(conf.label, 5, yHigh + 10);

            if (!buf.length) return;
            
            ctx.lineWidth = 2;
            ctx.beginPath();
            
            var hasStarted = false;
            // Draw digital as step
            for(var i=0; i<buf.length; i++) {
                var pt = buf[i];
                if (pt.t < tStart) continue;
                
                var x = ((pt.t - tStart) / timeWindow) * w;
                var currentY = pt.v ? yHigh : yBase;
                
                if (!hasStarted) {
                    ctx.moveTo(x, currentY);
                    hasStarted = true;
                } else {
                    // Step: horizontal to new x, then vertical to new y?
                    // Or vertical then horizontal? 
                    // To be accurate: hold value until change.
                    // But we only have sample points.
                    // Just lineTo is fine for high freq points, or step interpolation.
                    var prevPt = buf[i-1];
                    var prevY = prevPt.v ? yHigh : yBase;
                    ctx.lineTo(x, prevY); // Hold previous value
                    ctx.lineTo(x, currentY); // Jump
                }
            }
            ctx.stroke();
        });
        
        // Analog channels
        var minVal = 0;
        var maxVal = 100;
        
        this.config.forEach((c, idx) => {
            if (c.type !== 'analog') return;
            var buf = this.data[c.id];
            
            // Label in legend at bottom or top right?
            // Let's put legend at top of Analog area
            var legendX = w - 100;
            var legendY = yAnaStart + 10 + (idx - 3) * 12; // -3 to offset digital
            ctx.fillStyle = c.color;
            ctx.fillText(c.label, legendX, legendY);

            if (!buf.length) return;
            
            ctx.strokeStyle = c.color;
            ctx.lineWidth = 1;
            ctx.beginPath();
            
            var hasStarted = false;
            for(var i=0; i<buf.length; i++) {
                var pt = buf[i];
                if (pt.t < tStart) continue;
                
                var x = ((pt.t - tStart) / timeWindow) * w;
                var val = pt.v;
                // Clip visual range 
                var displayVal = Math.max(minVal, Math.min(val, maxVal));
                var y = yAnaStart + hAna - ((displayVal - minVal) / (maxVal - minVal)) * hAna;
                
                if (!hasStarted) {
                    ctx.moveTo(x, y);
                    hasStarted = true;
                } else {
                    ctx.lineTo(x, y);
                }
            }
            ctx.stroke();
        });
    }
};
