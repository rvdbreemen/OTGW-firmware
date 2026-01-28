# WebUI Security Best Practices - Quick Reference

This guide provides quick reference patterns for secure WebUI development in the OTGW-firmware project.

## ❌ NEVER DO THIS

### 1. innerHTML with User Data
```javascript
// ❌ DANGEROUS - XSS vulnerability
document.getElementById('userName').innerHTML = apiResponse.name;
document.getElementById('message').innerHTML = user.message;
container.innerHTML = `<div>${filename}</div>`;
```

### 2. Template Literals with User Data
```javascript
// ❌ DANGEROUS - Template injection
let html = `<tr><td>${json[i].name}</td></tr>`;
element.insertAdjacentHTML('beforeend', html);
```

### 3. fetch() without Error Handling
```javascript
// ❌ BAD - Silent failures
fetch('/api/data')
    .then(response => response.json())
    .then(data => { /* process */ });
```

### 4. getElementById without Null Check
```javascript
// ❌ BAD - Potential null reference
document.getElementById('myElement').textContent = 'value';
```

### 5. JSON.parse() without try-catch
```javascript
// ❌ BAD - Crashes on invalid JSON
const data = JSON.parse(userInput);
```

---

## ✅ ALWAYS DO THIS

### 1. Use textContent for User Data
```javascript
// ✅ SAFE - Automatically escapes HTML
const element = document.getElementById('userName');
if (element) {
    element.textContent = apiResponse.name;
}
```

### 2. Build DOM Elements for Complex Structures
```javascript
// ✅ SAFE - DOM methods prevent injection
const row = document.createElement('tr');
const cell = document.createElement('td');
cell.textContent = json[i].name;  // Safe text
row.appendChild(cell);
table.appendChild(row);
```

### 3. Always Handle fetch() Errors
```javascript
// ✅ GOOD - Complete error handling
fetch('/api/data')
    .then(response => {
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        // Process data
    })
    .catch(error => {
        console.error('Fetch failed:', error);
        // Update UI with error message
    });
```

### 4. Check Element Existence
```javascript
// ✅ GOOD - Safe element access
const element = document.getElementById('myElement');
if (element) {
    element.textContent = 'value';
}
```

### 5. Wrap JSON.parse() in try-catch
```javascript
// ✅ GOOD - Safe JSON parsing
try {
    const data = JSON.parse(userInput);
    if (data && typeof data === 'object') {
        // Process data
    }
} catch (e) {
    console.error('JSON parse error:', e);
    // Handle error gracefully
}
```

---

## Safe Patterns Library

### Pattern 1: Safe API Data Display

```javascript
function displayUserData(apiResponse) {
    const nameEl = document.getElementById('userName');
    const emailEl = document.getElementById('userEmail');
    
    if (nameEl) nameEl.textContent = apiResponse.name;
    if (emailEl) emailEl.textContent = apiResponse.email;
}
```

### Pattern 2: Safe Table Building

```javascript
function buildTable(data) {
    const table = document.createElement('table');
    
    data.forEach(item => {
        const row = table.insertRow();
        
        const nameCell = row.insertCell();
        nameCell.textContent = item.name;
        
        const valueCell = row.insertCell();
        valueCell.textContent = item.value;
    });
    
    container.appendChild(table);
}
```

### Pattern 3: Safe Link Creation

```javascript
function createSafeLink(url, text) {
    const link = document.createElement('a');
    link.href = url;  // Browser handles URL encoding
    link.textContent = text;  // Safe text
    link.target = '_blank';
    return link;
}
```

### Pattern 4: Comprehensive fetch() Wrapper

```javascript
async function safeFetch(url) {
    try {
        const response = await fetch(url);
        
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
        
        const contentType = response.headers.get('content-type');
        if (contentType && contentType.includes('application/json')) {
            return await response.json();
        } else {
            return await response.text();
        }
        
    } catch (error) {
        console.error('Fetch error:', error);
        // Update UI with error
        const errorEl = document.getElementById('errorMessage');
        if (errorEl) {
            errorEl.textContent = 'Failed to load data. Please try again.';
        }
        throw error; // Re-throw for caller to handle
    }
}
```

### Pattern 5: Safe Form Input Handling

```javascript
function getFormValue(fieldId) {
    const field = document.getElementById(fieldId);
    if (!field) {
        console.warn(`Field ${fieldId} not found`);
        return null;
    }
    
    if (field.type === 'checkbox') {
        return field.checked;
    } else {
        return field.value;
    }
}
```

---

## Security Checklist for Code Reviews

Use this checklist when reviewing or writing WebUI code:

- [ ] No `innerHTML` with user-controlled data
- [ ] No template literals (`${}`) with user data in HTML strings
- [ ] All `fetch()` calls have `.catch()` handlers
- [ ] All `fetch()` calls check `response.ok` before parsing
- [ ] All `JSON.parse()` calls wrapped in try-catch
- [ ] All `getElementById()` results checked for null
- [ ] No `eval()` or `Function()` constructor usage
- [ ] User input sanitized before display
- [ ] Error messages shown to user (not just console)
- [ ] Hardcoded strings use `F()` macro (backend) or are in PROGMEM

---

## Common Mistakes and Fixes

### Mistake 1: "It's just a message string, innerHTML is fine"
```javascript
// ❌ WRONG - Messages can contain malicious content
document.getElementById('message').innerHTML = apiData.message;

// ✅ RIGHT - Always use textContent
document.getElementById('message').textContent = apiData.message;
```

### Mistake 2: "The API won't return bad data"
```javascript
// ❌ WRONG - Never trust external data
const name = apiResponse.name;
container.innerHTML = `<p>${name}</p>`;

// ✅ RIGHT - Always sanitize
const p = document.createElement('p');
p.textContent = apiResponse.name;
container.appendChild(p);
```

### Mistake 3: "Users won't upload malicious filenames"
```javascript
// ❌ WRONG - Attackers will try
html += `<td>${filename}</td>`;

// ✅ RIGHT - Use DOM methods
const cell = row.insertCell();
cell.textContent = filename;
```

### Mistake 4: "Network errors are rare"
```javascript
// ❌ WRONG - They happen
fetch('/api/data').then(r => r.json()).then(process);

// ✅ RIGHT - Handle errors
fetch('/api/data')
    .then(r => {
        if (!r.ok) throw new Error(`HTTP ${r.status}`);
        return r.json();
    })
    .then(process)
    .catch(error => {
        console.error('Error:', error);
        showErrorToUser(error.message);
    });
```

---

## When innerHTML IS Acceptable

innerHTML can be used ONLY for:

1. **Static hardcoded strings:**
   ```javascript
   // ✅ OK - No user data
   container.innerHTML = '<div class="spinner">Loading...</div>';
   ```

2. **Already-escaped output from trusted escape function:**
   ```javascript
   // ✅ OK - Using escape function
   function escapeHtml(text) {
       const div = document.createElement('div');
       div.textContent = text;
       return div.innerHTML;
   }
   
   container.innerHTML = `<div>${escapeHtml(userInput)}</div>`;
   ```

---

## Summary

**Golden Rules:**
1. User data → `textContent` (NOT `innerHTML`)
2. Complex HTML → `createElement()` (NOT template strings)
3. fetch() → Always check `response.ok` + `.catch()`
4. getElementById() → Always check for `null`
5. JSON.parse() → Always wrap in `try-catch`

**When in doubt:** Use DOM methods, not string manipulation.

---

**For questions or clarifications, refer to:** [WEBUI_CODE_REVIEW.md](WEBUI_CODE_REVIEW.md)
