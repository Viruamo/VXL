FROM node:18-alpine

WORKDIR /app

# BẮT BUỘC: Phải copy cả package.json và package-lock.json vào trước
COPY package*.json ./

# Sau đó mới chạy npm ci
RUN npm ci --only=production

COPY . .

CMD ["node", "server.js"] # hoặc lệnh khởi chạy của bạn
