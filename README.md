# C++ Thread Pool Web Server with Live Dashboard

This is a high-performance, multithreaded web server written in C++ from scratch. It utilizes a thread pool to efficiently handle concurrent client connections and serves a dynamic, live-updating dashboard to monitor its own performance in real-time.

## Features

- **Multithreaded Architecture:** Uses a custom-built, fixed-size thread pool to manage worker threads, making it scalable and efficient.
- **Live Performance Dashboard:** A web-based UI that displays:
    - Real-time Active Threads count and Utilization percentage.
    - A visual grid representing the status (Busy/Idle) of every thread in the pool.
    - Total requests processed since server start.
    - Current connected users and peak connections.
    - Total closed connections.
    - Number of tasks waiting in the queue under heavy load.
- **Interactive File Server:** An integrated file requester that can list and serve media files (images, videos) from the server's local storage.
- **Detailed Terminal Logging:** Provides real-time, low-level logs showing which thread is handling which request from which client.
- **Built with Standard Libraries:** Created using fundamental C++ libraries for sockets, threading, and synchronization, without relying on heavy external frameworks.

## Project Structure