up:
	docker compose up --build -d

down:
	docker compose down --rmi all

logs:
	docker compose logs -f
