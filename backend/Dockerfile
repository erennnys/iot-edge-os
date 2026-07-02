FROM node:20-slim

# Install compilation tools for native node modules (better-sqlite3 compilation)
RUN apt-get update && apt-get install -y \
    python3 \
    make \
    g++ \
    sqlite3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy package files
COPY package*.json ./

# Install dependencies and build native extensions
RUN npm install --only=production

# Create data directory
RUN mkdir -p /app/data

# Copy source code
COPY . .

CMD ["node", "index.js"]
