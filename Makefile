up:
	docker compose up --build -d

down:
	docker compose down --rmi all

logs:
	docker compose logs -f

rmi:
	docker images -q | xargs docker rmi
