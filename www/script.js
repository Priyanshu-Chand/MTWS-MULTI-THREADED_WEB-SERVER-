// Configuration
const CONFIG = {
    totalThreads: 32,          // Maximum threads in your server
    updateInterval: 2000,      // Refresh every 2 seconds
    apiEndpoint: '/status'     // Endpoint served by your C++ backend
};

// --- UTILITY FUNCTIONS ---
function getColorClass(percent) {
    if (percent < 50) return 'green';
    if (percent < 80) return 'yellow';
    return 'red';
}

function formatNumber(num) {
    return (typeof num === 'number' ? num : 0).toLocaleString();
}

function updateLastUpdatedTime() {
    const now = new Date();
    document.getElementById('lastUpdated').textContent = now.toLocaleTimeString('en-IN', { hour12: false });
}

// --- DASHBOARD UPDATE LOGIC ---
function updateDashboard(data) {
    // Active Threads Card
    const threadsPercent = (data.activeThreads / CONFIG.totalThreads) * 100;
    document.getElementById('activeThreads').textContent = formatNumber(data.activeThreads);
    const threadsProgress = document.getElementById('threadsProgress');
    threadsProgress.style.width = threadsPercent + '%';
    threadsProgress.className = 'progress ' + getColorClass(threadsPercent);
    const queuedTasks = data.queuedTasks || 0;
    document.getElementById('threadsInfo').innerHTML = 
        `Total: ${CONFIG.totalThreads} | Utilization: ${threadsPercent.toFixed(1)}%<br>` +
        `<strong>Queued Tasks: ${formatNumber(queuedTasks)}</strong>`;

    // Active Requests Card
    document.getElementById('activeRequests').textContent = formatNumber(data.totalRequests);
    document.getElementById('requestsInfo').textContent = 
        `Requests processed: ${formatNumber(data.totalRequests)}`;

    // Connected Users Card
    document.getElementById('connectedUsers').textContent = formatNumber(data.connectedUsers);
    document.getElementById('usersInfo').textContent = 
        `Peak Connections: ${formatNumber(data.peakConnections)}`;

    // Closed Connections Card
    document.getElementById('closedConnections').textContent = formatNumber(data.closedConnections);
    document.getElementById('closedInfo').textContent = 
        `Connections closed: ${formatNumber(data.closedConnections)}`;

    // Thread Pool Visualization
    const threadBoxes = document.querySelectorAll('.thread-box');
    for (let i = 0; i < threadBoxes.length; i++) {
        threadBoxes[i].className = i < data.activeThreads ? 'thread-box busy' : 'thread-box idle';
    }

    updateLastUpdatedTime();
}

async function fetchServerStats() {
    try {
        const response = await fetch(CONFIG.apiEndpoint);
        if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
        return await response.json();
    } catch (error) {
        console.error('Error fetching server stats:', error);
        return { activeThreads: 0, totalRequests: 0, connectedUsers: 0, closedConnections: 0, peakConnections: 0, queuedTasks: 0 };
    }
}

async function refreshDashboard() {
    const data = await fetchServerStats();
    updateDashboard(data);
}

// --- FILE REQUESTER LOGIC ---
async function populateFileSelector() {
    const select = document.getElementById('file-select');
    try {
        const response = await fetch('/api/files');
        const files = await response.json();
        
        select.innerHTML = '';
        if (files.length === 0) {
            select.innerHTML = '<option>No media files found.</option>';
            return;
        }
        files.forEach(file => {
            const option = document.createElement('option');
            option.value = file;
            option.textContent = file;
            select.appendChild(option);
        });
    } catch (error) {
        console.error('Error fetching file list:', error);
        select.innerHTML = '<option>Could not load files.</option>';
    }
}

function handleFileRequest() {
    const select = document.getElementById('file-select');
    const display = document.getElementById('media-display');
    const filename = select.value;
    if (!filename || filename.startsWith('Could not') || filename.startsWith('No media')) {
        display.innerHTML = '<p>Please select a valid file.</p>';
        return;
    }
    display.innerHTML = '';
    const fileUrl = `/media/${filename}`;
    if (filename.endsWith('.mp4')) {
        const video = document.createElement('video');
        video.src = fileUrl;
        video.controls = true;
        display.appendChild(video);
    } else {
        const img = document.createElement('img');
        img.src = fileUrl;
        img.alt = filename;
        display.appendChild(img);
    }
}

// --- INITIALIZATION LOGIC ---
function initDashboard() {
    const visualizationContainer = document.getElementById('thread-pool-visualization');
    for (let i = 0; i < CONFIG.totalThreads; i++) {
        const threadBox = document.createElement('div');
        threadBox.className = 'thread-box idle';
        visualizationContainer.appendChild(threadBox);
    }
    
    refreshDashboard(); // Run once immediately on load
    setInterval(refreshDashboard, CONFIG.updateInterval); // Then start the 2-second loop
    console.log('Dashboard initialized at ' + new Date().toLocaleTimeString());
}

// This is the single entry point that runs when the page is ready.
document.addEventListener('DOMContentLoaded', () => {
    initDashboard();
    populateFileSelector();
    document.getElementById('request-btn').addEventListener('click', handleFileRequest);
});